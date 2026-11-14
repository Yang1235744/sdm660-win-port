/*
 * ClocksGpcd.c — SDM660 GPLL/VCO clock tree driver for UEFI DXE phase
 *
 * The SDMLinux EFI project needs to route the primary GPLL (1.84 GHz VCO)
 * output to the CPU clusters BEFORE handing control to Windows. This is
 * because `bootmgfw.efi` assumes a pre-initialized CPU clock domain; without
 * it, early instruction fetches fail with bus errors on cores running out
 * of sync.
 *
 * Hardware reference:
 *   - Qualcomm SDM660 TRM §23 "Global Clock and Power Controller"
 *   - linux/kernel/msm/arch/arm64/boot/dts/qcom/sdm660.dtsi  node &gcc
 *
 * The clock table below mirrors the SDM660 PLL mux hierarchy as published
 * in the public kernel DTSI. Register offsets are PHYSICAL MMIO addresses;
 * read them via READ_REGISTER_UINT32/16/8 before use.
 */

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../UefiPlatform/Include/Hardware/Platform.h"

/* ── Register stride for each GCC child block (from TRM Table 7-12) ── */
#define GCC_CHILD_OFFSET      0x400ULL

/* Clock identifiers used by SDM660 clock framework */
typedef enum {
  CLK_GPLL0            = 0,       /* Primary 1.84 GHz VCO             */
  CLK_GPLL0_EARLY      = 1,       /* Early-output variant (lower jitter) */
  CLK_BIMC_CLK         = 2,       /* Bus Interface Master Clock        */
  CLK_CPU0_CLK         = 3,       /* A53 Cluster 0 primary clock       */
  CLK_CPU1_CLK         = 4,       /* A73 Cluster 0 primary clock       */
  CLK_UFSHCI_CORE_CLK  = 5,       /* Storage host controller core     */
  CLK_UFSHCI_ICE_CLK   = 6,       /* UFS inline crypto engine clock   */
  CLK_MDSS_VSYNC_CLK   = 7,       /* Display timing generator         */
  CLK_QUP_SPI1_CLK     = 8,       /* QUP SPI1 (Wi-Fi interface)       */
  CLK_QUP_I2C1_CLK     = 9,       /* QUP I2C1 (touch panel)           */
  CLK_BLSP1_UART2_CLK  = 10,      /* Bluetooth LE dual-use UART       */
  CLK_NUM              = 11
} GccClockId;

/* ─── Public API ─────────────────────────────────────────────── */

/**
 * EnableClock — gate or ungate a single peripheral clock by ID.
 *
 * Returns EFI_SUCCESS if the clock was already enabled; otherwise
 * configures the appropriate GCC_CTL register and flushes the write.
 *
 * ⚠️ WARNING: Calling this with an invalid clock ID will corrupt the
 *              GCC state machine. Always validate against CLK_NUM first.
 */
EFI_STATUS
EFIAPI
EnableClock (
  IN GccClockId Id,
  IN BOOLEAN    Enable
  )
{
  UINT32 Reg;
  UINTN Offset;

  if (Id >= CLK_NUM)
    return EFI_INVALID_PARAMETER;

  /* GCC child blocks are at fixed 0x400 strides; base = GCC_BASE + Id*0x400 */
  Offset = GCC_BASE + ((UINTN)Id * GCC_CHILD_OFFSET);

  Reg = READ_REGISTER_UINT32 (Offset + 0x000);  /* CTL register */
  if (Enable)
    WRITE_REGISTER_UINT32 (Offset + 0x000, Reg | BIT (0));  /* SET bit 0 */
  else
    WRITE_REGISTER_UINT32 (Offset + 0x000, Reg & ~BIT (0));  /* CLR bit 0 */

  /* Flush through Cache maintenance so the hardware sees the write immediately */
  CleanDataCacheRange ((UINTN)Offset, 4);

  return EFI_SUCCESS;
}

/**
 * SetCpuClockSource — select which PLL feeds CPU cluster clocks.
 *
 * SDM660 supports:
 *   - GPLL0 main (for normal performance mode)
 *   - SLEEP_CLK (for power-down / suspend)
 *   - XO (crystal oscillator, fallback only)
 *
 * Switching the parent clock while the CPU is active requires careful
 * RPM (Resource Power Manager) coordination; this function assumes the
 * RPM has already been informed via a SPMI IPC message.
 */
EFI_STATUS
EFIAPI
SetCpuClockSource (
  IN UINT32 SourceXor   /* 0=GPLL0, 1=SLEEP_CLK, 2=XO */
  )
{
  UINT32 Reg;

  Reg = READ_REGISTER_UINT32 (GCC_CORE_CLK_CFG);
  Reg = (Reg & ~BIT (0)) | (SourceXor & 0x3);  /* Replace lower 2 bits */
  WRITE_REGISTER_UINT32 (GCC_CORE_CLK_CFG, Reg);
  CleanDataCacheRange (GCC_CORE_CLK_CFG, 4);

  return EFI_SUCCESS;
}

/**
 * GetClockStatus — returns TRUE if the given clock is currently gated off.
 */
BOOLEAN
EFIAPI
IsClockGated (
  IN GccClockId Id
  )
{
  UINTN Offset = GCC_BASE + ((UINTN)Id * GCC_CHILD_OFFSET);
  UINT32 Reg = READ_REGISTER_UINT32 (Offset + 0x000);
  return (Reg & BIT (0)) == 0;  /* Bit 0 clear = disabled/gated */
}
