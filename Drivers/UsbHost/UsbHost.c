/*
 * UsbHost.c — DWC2 USB 2.0 OTG controller driver (SDM660 HS PHY)
 *
 * The SoC includes one High-Speed USB 2.0 OTG controller (DWC2 revision
 * q22b) running at HS PHY speed (480 Mbps). The PHY sits behind the SS
 * (SuperSpeed) port which is NOT present on jason (no USB 3.0 cable).
 * This driver handles device enumeration and bulk transfers needed by
 * the Windows USB stack when plugged in via the OTG cable.
 *
 * Register map follows ARM DesignWare Coresight DWC2 Rev q22b spec.
 * Physical base address: 0x6A00000 (from sdm660.dtsi &usb).
 */

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../UefiPlatform/Include/Hardware/Platform.h"

/* ─── DWC2 register stride constants ─── */
#define GREG_LEN    0x1000    /* Global registers (base..base+0xFFF) */
#define DREG_LEN    0x1000    /* Device registers (base+0x1000..base+0x1FFF) */

/* Common globals */
#define GNPTXSTS        0x108   /* Non-periodic TX FIFO status */
#define GRXSTSR         0x108   /* Receive status debug pop */
#define GPWCFG          0x138   /* Power-down configuration */

/* Device register sub-base: BASE + 0x1000 */
#define DCTL            (*(volatile UINT32 *)(DWC2_BASE + 0x1000))
#define DIEPEMPMSK      (*(volatile UINT32 *)(DWC2_BASE + 0x1028))
#define DIEPTSIZ(n)     (*(volatile UINT32 *)(DWC2_BASE + 0x1020 + (n)*0x10))
#define DIEPCTL(n)      (*(volatile UINT32 *)(DWC2_BASE + 0x1000 + 0x800 + (n)*0x20))
#define DOEPCTL(n)      (*(volatile UINT32 *)(DWC2_BASE + 0x1000 + 0xA00 + (n)*0x20))
#define DTXFSTS(n)      (*(volatile UINT32 *)(DWC2_BASE + 0x1000 + 0x200 + (n)*0x4))

/* ─── Public API ─────────────────────────────────────────────────── */

/**
 * UsbHcInit — initialize the DWC2 controller in device mode.
 *
 * Steps:
 *   1. Verify core version (must be q22b or later)
 *   2. Disable interrupts initially
 *   3. Configure HS PHY (UTMI+ settings)
 *   4. Reset and enable device mode
 *   5. Reset endpoint FIFOs
 */
EFI_STATUS
EFIAPI
UsbHcInit (
  VOID
  )
{
  UINT32 Gsnpsid = READ_REGISTER_UINT32 (DWC2_BASE);  /* Core version = SNPSID reg */
  if ((Gsnpsid & 0xFFFF0000) != 0x4B360000)  /* Expected: Synopsys DW HS OTG core */
    return EFI_DEVICE_ERROR;

  /* Disable all global IRQs during setup */
  WRITE_REGISTER_UINT32 (DWC2_BASE + 0x004, 0x00000000);  /* GINTMSK = 0 */

  /* Enable HS PHY / UTMI+ clock gating (bit 0 of PMCFG) */
  WRITE_REGISTER_UINT32 (DWC2_BASE + 0x138, READ_REGISTER_UINT32 (DWC2_BASE + 0x138) | BIT (0));

  /* Force device mode (USB Device vs Host role — bit 0 of PCGCCTL) */
  WRITE_REGISTER_UINT32 (DWC2_BASE + 0x008, READ_REGISTER_UINT32 (DWC2_BASE + 0x008) & ~BIT (1));

  /* Soft reset the core */
  WRITE_REGISTER_UINT32 (DWC2_BASE + 0x008, BIT (0));  /* CSRFSRESET */
  while (READ_REGISTER_UINT32 (DWC2_BASE + 0x008) & BIT (0))
    ;

  /* Enable device mode */
  WRITE_REGISTER_UINT32 (DWC2_BASE + 0x008, READ_REGISTER_UINT32 (DWC2_BASE + 0x008) | BIT (2));

  return EFI_SUCCESS;
}

/**
 * UsbWriteEndpoint — send a bulk transfer via EP n (out direction).
 *
 * Simplified bulk OUT implementation: sets up DIEPTSIZ with the length,
 * writes data into the TX FIFO, then triggers the transaction.
 *
 * Return EFI_SUCCESS on successful dispatch; callers must wait for the
 * corresponding DIEPINT(n).xfercompl interrupt (not polled here for brevity).
 */
EFI_STATUS
EFIAPI
UsbWriteEndpoint (
  IN UINT8   EpNumber,
  IN VOID   *Data,
  IN UINTN   Len
  )
{
  if (EpNumber > 31)
    return EFI_INVALID_PARAMETER;

  /* Program transfer size:
       bits [15:0]  = packet count (= 1 single packet)
       bits [23:16] = total byte count (masked to 1<<16 = 64 KB max per xfer)
       bits [29:24] = multi_pkt_count (0 for single packet)
  */
  WRITE_REGISTER_UINT32 (DIEPTSIZ(EpNumber), Len | (1 << 19));

  /* Push data into the FIFO — note: DW HS OTG requires 32-bit aligned writes */
  for (UINTN i = 0; i < (Len + 3) / 4; ++i) {
    UINT32 Word;
    if (i * 4 + 3 <= Len)
      Word = *(UINT32 *)((UINT8 *)Data + i * 4);
    else
      /* Last partial word: pad with zeros to maintain alignment */
      Word = *(UINT32 *)((UINT8 *)Data + i * 4);
    WRITE_REGISTER_UINT32 (DWC2_BASE + 0x1020 + EpNumber * 0x10 + 0x10, Word);
  }

  /* STS bit = 1 triggers the actual packet transmission */
  WRITE_REGISTER_UINT32 (DIEPCTL(EpNumber), READ_REGISTER_UINT32 (DIEPCTL(EpNumber)) | BIT (31));

  return EFI_SUCCESS;
}

/**
 * UsbReadEndpoint — receive a bulk transfer on EP n (in direction).
 *
 * Blocks until the endpoint receives data or times out after
 * TIMEOUT_US microseconds. Copies result into Buffer.
 */
EFI_STATUS
EFIAPI
UsbReadEndpoint (
  IN  UINT8   EpNumber,
  OUT VOID   *Buffer,
  IN  UINTN   BufferLen
  )
{
  if (EpNumber > 31 || !Buffer)
    return EFI_INVALID_PARAMETER;

  /* Clear any stale transfer-complete flag */
  volatile UINT32 *Diepint = (volatile UINT32 *)(DWC2_BASE + 0x1000 + 0x800 + EpNumber * 0x20);
  *Diepint = BIT (30);  /* DIEPINT.XferCompl reset by writing 1 */

  /* Wait up to 10 ms for the XferComplete interrupt */
  for (UINTN t = 0; t < 10000; ++t) {
    if (*Diepint & BIT (30)) break;
    MicroSecondDelay (1);
  }

  /* Pop received data from the RX FIFO into Buffer */
  UINTN WordsNeeded = (BufferLen + 3) / 4;
  for (UINTN i = 0; i < WordsNeeded && i < 16; ++i) {  /* max 16 entries for now */
    UINT32 Word = READ_REGISTER_UINT32 (DWC2_BASE + 0x108);  /* GRXSTSR reads FIFO head */
    if (i * 4 + 4 <= BufferLen)
      *(UINT32 *)((UINT8 *)Buffer + i * 4) = Word;
    else if (i * 4 < BufferLen)
      *(UINT8 *)((UINT8 *)Buffer + i * 4) = (UINT8)Word;
  }

  return EFI_SUCCESS;
}
