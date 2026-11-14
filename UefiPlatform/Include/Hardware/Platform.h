/**
 * SDM660 Platform Header — Register Base Addresses & Peripheral Maps
 *
 * Device: Xiaomi Mi Note 3 (codename "jason")
 * SoC:    Qualcomm Snapdragon 660 (SDM660)
 * Boot CPU: A53 CPU0 (little core)
 * Memory:  LPDDR4-4GB; memory base = 0x80000000 (AArch64 physical)
 *
 * All addresses below are PHYSICAL bus addresses used by Linux device-tree
 * nodes. They come from reverse-engineering sdm660.dtsi shipped with
 * the kernel sources at https://android.googlesource.com/kernel/msm/+/refs/tags/android-8.1.0_r0.37/.
 *
 * ⚠️ WARNING: Some values are ESTIMATED from public references only.
 *              Verify each against a real dmesg dump before hardening.
 */
#pragma once

#include <Uefi.h>
#include <Library/IoLib.h>

/* ────────────────────────────────────────────────────────────────
 *  Boot CPU & Core Init
 * ──────────────────────────────────────────────────────────────── */

#define SDM660_CORE0_PHYS         0x80000000ULL   /* first instruction runs here */
#define SDM660_RAM_BASE           0x80000000ULL
#define SDM660_RAM_SIZE           0x100000000ULL  /* 4 GB total */

/* SPMI PMIC address space — PMI632 registers accessible via SPU */
#define SPMI_BUS_BASE             0xAA000000ULL
#define PMI632_MFD_CTRL_BASE      0xAA010000ULL   /* MFD control block base */
#define PMI632_LDO0_CTRL          (PMI632_MFD_CTRL_BASE + 0x1000)
#define PMI632_BUCK0_CTRL         (PMI632_MFD_CTRL_BASE + 0x1200)
#define PMI632_GPIO0_BASE         (PMI632_MFD_CTRL_BASE + 0x1400)

/* QUP SPI/I2C controllers */
#define QUP_SPI1_BASE             0x164B0000ULL   /* QUP#1 (SPI1, WiFi interface) */
#define QUP_I2C1_BASE             0x164C0000ULL   /* QUP#2 (I2C1, touch panel)   */

/* ────────────────────────────────────────────────────────────────
 *  UART Console (debug output)
 * ──────────────────────────────────────────────────────────────── */
#define BLSP1_UART2_BASE          0x13C00000ULL   /* Bluetooth LE / UART2 dual-use pin */
#define BLSP1_UART2_TX            *(volatile UINT32 *)(BLSP1_UART2_BASE + 0x0A004)
#define BLSP1_UART2_RX            *(volatile UINT32 *)(BLSP1_UART2_BASE + 0x0A008)

/* ────────────────────────────────────────────────────────────────
 *  Clock Controller (GCC — Global Clock Control)
 *
 *  GPLL0 is the primary 1.84 GHz VCO for big cores; its output feeds
 *  the Kryo260 cluster via GCC_VFP_CFG / GCC_VMP_CFG registers.
 * ──────────────────────────────────────────────────────────────── */
#define GCC_BASE                  0x01B80000ULL
#define GCC_GPLL0_CTRL            (GCC_BASE + 0x00000)
#define GCC_GPLL0_MODE            (GCC_BASE + 0x00004)
#define GCC_CORE_CLK_CFG          (GCC_BASE + 0x00020)     /* A53/A73 clock mux select  */
#define GCC_BOOT_CNTRL            (GCC_BASE + 0x00024)     /* boot-control partition sel */

/* GPLL lock status bit position */
#define GPLL_LOCK_STATUS_BIT      31

/* ────────────────────────────────────────────────────────────────
 *  GPIO Controller (TLMM — Top Level Mode Mux)
 * ──────────────────────────────────────────────────────────────── */
#define TLMM_BASE                 0x1000000ULL
#define TLMM_PIN_CFG_OFFSET(n)    ((n) * 0x00008)          /* per-pin config register stride */
#define TLMM_FUNC_SELECT_SHIFT    0
#define TLMM_INPUT_ENABLE         BIT(9)
#define TLMM_OUTPUT_DISABLE       BIT(8)
#define TLMM_PULL_NONE            0
#define TLMM_PULL_DOWN            2
#define TLMM_DRIVE_STRENGTH_8MA   3

/* ────────────────────────────────────────────────────────────────
 *  Interconnect (SLPC — system low power controller)
 * ──────────────────────────────────────────────────────────────── */
#define RPMH_CC_BASE              0x03400000ULL
#define SLPC_MEM_CFG              (RPMH_CC_BASE + 0x00100)

/* ────────────────────────────────────────────────────────────────
 *  Storage (eMMC / UFS)
 *
 *  SDM660 supports both eMMC 5.1 and UFS 2.1 via a single SoC host
 *  controller. jason uses UFS 2.1 (Samsung/Kioxia).
 * ──────────────────────────────────────────────────────────────── */
#define UFS_HC_BASE               0x17A00000ULL
#define UFS_HC_INTR_STAT          *(volatile UINT32 *)(UFS_HC_BASE + 0x000)
#define UFS_HC_DBICFG0            *(volatile UINT32 *)(UFS_HC_BASE + 0x080)
#define UFS_HC_TMFLUSH0           *(volatile UINT32 *)(UFS_HC_BASE + 0x0A0)

/* eMMC host — fallback if UFS not detected */
#define EMMC_HC_BASE              0x17A10000ULL
#define EMMC_HC_CMD               *(volatile UINT32 *)(EMMC_HC_BASE + 0x000)
#define EMMC_HC_ARG               *(volatile UINT32 *)(EMMC_HC_BASE + 0x004)
#define EMMC_HC_RESP0             *(volatile UINT32 *)(EMMC_HC_BASE + 0x010)

/* ────────────────────────────────────────────────────────────────
 *  Display (MDSS — Mobile Display Sub System)
 *
 *  jason has two panels: OLED primary (Himax HM8202E) + secondary
 *  ambient-light sensor. MDSS commands flow through MDP7 DMA path.
 * ──────────────────────────────────────────────────────────────── */
#define MDSS_BASE                 0xAF00000ULL
#define MDSS_BLK_BASE             (MDSS_BASE + 0x00000)   /* brightness/backlight ctrl   */
#define MDSS_VSYNC_BASE           (MDSS_BASE + 0x10000)   /* timing generator base       */
#define MDSS_DMA_BASE             (MDSS_BASE + 0x20000)   /* primary DMA pipe base       */
#define DISP_PLL_BASE             (MDSS_BASE + 0x30000)   /* pixel PLL reference clock   */

/* Panel resolution (jason OEM spec) */
#define PANEL_WIDTH               1080
#define PANEL_HEIGHT              2160
#define PANELbpp                  32                      /* XRGB8888 native mode        */

/* ────────────────────────────────────────────────────────────────
 *  Touch & Sensors (I2C on QUP #2)
 * ──────────────────────────────────────────────────────────────── */
#define TOUCH_I2C_SLAVE_ADDR      0x38                    /* TrueTouch Classic TSPP      */
#define SENSOR_HUB_BASE           0x17B00000ULL           /* Hexagon DSP side-channel    */
#define SHMEM_BASE                0x88000000ULL           /* shared memory with modem    */

/* ────────────────────────────────────────────────────────────────
 *  Wi-Fi / Bluetooth (QCA6174 on PCIe)
 *
 *  The QMI/WCNSS firmware blob is loaded from vendor recovery image.
 *  This project assumes we already have ath10k-compatible firmware
 *  blobs extracted from the Android partition layout.
 * ──────────────────────────────────────────────────────────────── */
#define WLAN_PCIE_SLOT            1                       /* BDF: 0000:01:00.0           */
#define BT_PCIE_SLOT              2                       /* BDF: 0000:02:00.0           */
#define ATH10K_FW_PATH            "/vendor/firmware/ath10k/QCA6174/"

/* ────────────────────────────────────────────────────────────────
 *  USB 2.0 OTG (DWC2 @ HS PHY)
 * ──────────────────────────────────────────────────────────────── */
#define DWC2_BASE                 0x6A00000ULL
#define DWC2_GLOBAL_REG           *(volatile UINT32 *)(DWC2_BASE + 0x000)
#define DWC2_GNJNKCFG             *(volatile UINT32 *)(DWC2_BASE + 0x004)
#define DWC2_DSTS                 *(volatile UINT32 *)(DWC2_BASE + 0x008)

/* ────────────────────────────────────────────────────────────────
 *  Power Management — Watchdog Timer (WDT)
 * ──────────────────────────────────────────────────────────────── */
#define WDT_BASE                  0x10500000ULL
#define WDT_EN                    *(volatile UINT32 *)(WDT_BASE + 0x000)
#define WDT_LOAD                  *(volatile UINT32 *)(WDT_BASE + 0x004)
#define WDT_COUNTER               *(volatile UINT32 *)(WDT_BASE + 0x008)

/* ────────────────────────────────────────────────────────────────
 *  Boot ROM escape — unlock boot control partition from userland
 *
 *  Writing to BOOT_CNTRL requires the corresponding SPL (Secure
 *  Partition Loader) to be enabled first. This function is called
 *  inside the UEFI PEI phase to open the boot partition after TPM
 *  attestation succeeds.
 * ──────────────────────────────────────────────────────────────── */
STATIC
UINTN
EFIAPI
PllUnlockBootControlPartition (
  VOID
  )
{
  /* 1. Wait for GPLL lock status bit [31] to go high */
  while (!((READ_REGISTER_UINT32 (GCC_GPLL0_MODE)) & BIT (GPLL_LOCK_STATUS_BIT))) {
    MicroSecondDelay (1);
  }

  /* 2. Re-program BOOT_CNTRL to allow reading of partition metadata */
  WRITE_REGISTER_UINT32 (GCC_BOOT_CNTRL, READ_REGISTER_UINT32 (GCC_BOOT_CNTRL) | BIT (4));

  return GCC_BASE;
}
