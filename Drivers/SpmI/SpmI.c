/*
 * jason SPMI PMIC arbiter (v2) driver.
 *
 * Verified against drivers/spmi/spmi-pmic-arb.c (pmic_arb_offset_v2,
 * pmic_arb_fmt_cmd_v2, pmic_arb_find_apid) and the jason DTB node
 * qcom,spmi@800f000:
 *
 *   core   0x0800F000 / 0x1000  version + PPID->APID table
 *   chnls  0x08400000           write channels
 *   obsrvr 0x09400000           read channels
 *   cnfg   0x0800A000           APID ownership table
 *   qcom,ee = 0
 *
 * Kept deliberately small: byte oriented transfers of 1..8 bytes, which is
 * everything the key and battery drivers need. Interrupt handling, the PIC and
 * the observer FIFO are not implemented; all waits are polled and bounded.
 */

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Hardware/Platform.h>
#include "SpmI.h"

#define SPMI_TIMEOUT_US   1000U
#define SPMI_MAX_TRANSFER 8U

EFI_GUID  gJasonSpmIProtocolGuid = {
  0x6A1B7C3D, 0x2F64, 0x4A19, { 0x9C, 0x58, 0x1D, 0xE3, 0x7B, 0x40, 0xC2, 0x55 }
};

/**
  Print a formatted line without relying on DebugLib varargs.
**/
STATIC
VOID
SpmILog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  CHAR8    Buffer[160];
  VA_LIST  Marker;

  VA_START (Marker, Format);
  AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);
  DEBUG ((DEBUG_INFO, "SpmI: %a\n", Buffer));
}

/**
  Translate a SPMI slave id / peripheral address pair into a PMIC arbiter APID.

  The arbiter maps PPID (slave id + peripheral id) to an APID through the table
  at core + 0x800, four bytes per APID, with the PPID in bits 11:0 of the
  register value shifted right by 8. Entries that read back as zero are unused.
**/
STATIC
EFI_STATUS
SpmIFindApid (
  IN  UINT8   Sid,
  IN  UINT16  Addr,
  OUT UINT32  *Apid
  )
{
  UINT32  Ppid;
  UINT32  Index;
  UINT32  Reg;

  Ppid = ((UINT32)Sid << 8) | ((UINT32)(Addr >> 8) & 0xFFU);

  for (Index = 0; Index < SPMI_APID_MAX; Index++) {
    Reg = MmioRead32 ((UINTN)SPMI_CORE_BASE + SPMI_APID_MAP_BASE + 4U * Index);
    if (Reg == 0) {
      continue;
    }
    if (((Reg >> 8) & 0xFFFU) == Ppid) {
      *Apid = Index;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Run one SPMI read or write and wait for the channel to report completion.
**/
STATIC
EFI_STATUS
SpmITransfer (
  IN UINT8        Sid,
  IN UINT16       Addr,
  IN OUT VOID     *Buffer,
  IN UINTN        Length,
  IN BOOLEAN      Write
  )
{
  EFI_STATUS  Status;
  UINT32      Apid;
  UINTN       Base;
  UINT32      Offset;
  UINT32      Word;
  UINT32      Status32;
  UINTN       Wait;
  UINT8       *Bytes;

  if ((Length == 0) || (Length > SPMI_MAX_TRANSFER)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SpmIFindApid (Sid, Addr, &Apid);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Base   = Write ? (UINTN)SPMI_CHNLS_BASE : (UINTN)SPMI_OBSRVR_BASE;
  Offset = SPMI_EE * SPMI_EE_STRIDE + Apid * SPMI_APID_STRIDE;
  Bytes  = (UINT8 *)Buffer;

  if (Write) {
    Word = 0;
    CopyMem (&Word, Bytes, (Length > 4) ? 4 : Length);
    MmioWrite32 (Base + Offset + SPMI_REG_WDATA0, Word);
    if (Length > 4) {
      Word = 0;
      CopyMem (&Word, Bytes + 4, Length - 4);
      MmioWrite32 (Base + Offset + SPMI_REG_WDATA1, Word);
    }
  }

  MmioWrite32 (
    Base + Offset + SPMI_REG_CMD,
    SPMI_CMD_OPCODE (Write ? SPMI_OPC_EXT_WRITEL : SPMI_OPC_EXT_READL, Addr, Length)
    );

  Status32 = 0;
  for (Wait = 0; Wait < SPMI_TIMEOUT_US; Wait++) {
    Status32 = MmioRead32 (Base + Offset + SPMI_REG_STATUS);
    if ((Status32 & SPMI_STATUS_DONE) != 0) {
      break;
    }
    MicroSecondDelay (1);
  }

  if ((Status32 & SPMI_STATUS_DONE) == 0) {
    SpmILog ("sid %d addr 0x%x: timeout, status 0x%x", (UINTN)Sid, (UINTN)Addr, (UINTN)Status32);
    return EFI_TIMEOUT;
  }

  if ((Status32 & (SPMI_STATUS_FAILURE | SPMI_STATUS_DENIED | SPMI_STATUS_DROPPED)) != 0) {
    SpmILog ("sid %d addr 0x%x: transaction failed, status 0x%x", (UINTN)Sid, (UINTN)Addr, (UINTN)Status32);
    return EFI_DEVICE_ERROR;
  }

  if (!Write) {
    Word = MmioRead32 (Base + Offset + SPMI_REG_RDATA0);
    CopyMem (Bytes, &Word, (Length > 4) ? 4 : Length);
    if (Length > 4) {
      Word = MmioRead32 (Base + Offset + SPMI_REG_RDATA1);
      CopyMem (Bytes + 4, &Word, Length - 4);
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
SpmIRead (
  IN  JASON_SPMI_PROTOCOL  *This,
  IN  UINT8                Sid,
  IN  UINT16               Addr,
  OUT VOID                 *Buffer,
  IN  UINTN                Length
  )
{
  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  return SpmITransfer (Sid, Addr, Buffer, Length, FALSE);
}

STATIC
EFI_STATUS
EFIAPI
SpmIWrite (
  IN JASON_SPMI_PROTOCOL  *This,
  IN UINT8                Sid,
  IN UINT16               Addr,
  IN CONST VOID           *Buffer,
  IN UINTN                Length
  )
{
  if ((This == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  return SpmITransfer (Sid, Addr, (VOID *)Buffer, Length, TRUE);
}

STATIC JASON_SPMI_PROTOCOL  mSpmIProtocol = {
  JASON_SPMI_PROTOCOL_SIGNATURE,
  SpmIRead,
  SpmIWrite
};

EFI_STATUS
EFIAPI
SpmIDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT8       Probe;

  if (!FeaturePcdGet (PcdPmicEnabled)) {
    return EFI_SUCCESS;
  }

  /*
   * Probe the arbiter with a single byte read of the PM660 PON real time
   * status. A DONE without FAILURE/DENIED means the APID table and the channel
   * window are both programmed the way the bootloader left them.
   */
  Probe  = 0;
  Status = SpmITransfer (PM660_SID, PMIC_PON_RT_STS, &Probe, 1, FALSE);
  if (EFI_ERROR (Status)) {
    SpmILog ("arbiter not usable (%r), PMIC drivers will be skipped", Status);
    return EFI_SUCCESS;
  }

  SpmILog ("online, PM660 PON status 0x%02x", (UINTN)Probe);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gJasonSpmIProtocolGuid,
                  &mSpmIProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    SpmILog ("protocol install failed (%r)", Status);
  }

  return EFI_SUCCESS;
}
