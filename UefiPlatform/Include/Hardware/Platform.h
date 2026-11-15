/*
 * jason (Xiaomi Mi Note 3 / SDM660) register map.
 *
 * Every value below is either taken from the DTB extracted out of the matching
 * boot.img (see Docs/hardware-notes.md) or from the mainline Linux driver for
 * the same SoC. Values are addresses/offsets, not proof that a driver sequence
 * is complete. Nothing here may be used to write flash or unlock anything.
 */
#pragma once

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>

/* ---- CPU / memory -------------------------------------------------- */
#define SDM660_RAM_BASE           0x80000000ULL

/* ---- GICv3 (DTB: /soc/interrupt-controller@17a00000) --------------- */
#define JASON_GICD_BASE           0x17A00000ULL
#define JASON_GICD_SIZE           0x00010000ULL
#define JASON_GICR_BASE           0x17B00000ULL
#define JASON_GICR_SIZE           0x00100000ULL
#define JASON_GICR_STRIDE         0x00020000ULL

/* ---- GCC (DTB: /soc/clock-controller@100000, qcom,gcc-sdm660) ------ */
#define GCC_BASE                  0x00100000ULL

/* GCC offsets verified against drivers/clk/qcom/gcc-sdm660.c. */
#define GCC_UFS_BCR               0x75000U  /* UFS block reset            */
#define GCC_UFS_GDSCR             0x75004U  /* UFS power domain           */
#define GCC_UFS_AXI_CBCR          0x75008U  /* gcc_ufs_axi_clk            */
#define GCC_UFS_AHB_CBCR          0x7500C  /* gcc_ufs_ahb_clk            */
#define GCC_UFS_TX_SYMBOL_CBCR    0x75010U  /* gcc_ufs_tx_symbol_0_clk    */
#define GCC_UFS_RX_SYMBOL_CBCR    0x75014U  /* gcc_ufs_rx_symbol_0_clk    */
#define GCC_UFS_AXI_CMD_RCGR      0x75018U  /* ufs_axi_clk_src            */
#define GCC_AGGRE2_UFS_AXI_CBCR   0x75034U  /* gcc_aggre2_ufs_axi_clk     */
#define GCC_UFS_UNIPRO_CBCR       0x76008U  /* gcc_ufs_unipro_core_clk    */
#define GCC_UFS_ICE_CBCR          0x7600C  /* gcc_ufs_ice_core_clk       */
#define GCC_UFS_ICE_CMD_RCGR      0x76010U  /* ufs_ice_core_clk_src       */
#define GCC_UFS_UNIPRO_CMD_RCGR   0x76028U  /* ufs_unipro_core_clk_src    */
#define GCC_UFS_PHY_AUX_CBCR      0x76040U  /* gcc_ufs_phy_aux_clk        */
#define GCC_UFS_CLKREF_CBCR       0x88008U  /* gcc_ufs_clkref_clk         */

/* ---- UFS (DTB: ufshc@1da4000 / ufsphy@1da7000 / ufsice@1db0000) ---- */
#define UFS_HC_BASE               0x01DA4000ULL
#define UFS_HC_SIZE               0x00003000ULL
#define UFS_PHY_BASE              0x01DA7000ULL
#define UFS_PHY_SIZE              0x00000DB8ULL
#define UFS_ICE_BASE              0x01DB0000ULL
#define UFS_ICE_SIZE              0x00008000ULL
#define UFS_SPI_IRQ               0x109

/* ---- QUP I2C for the touch panel (DTB: i2c@c178000) ---------------- */
#define QUP_TOUCH_I2C_BASE        0x0C178000ULL
#define QUP_TOUCH_I2C_SIZE        0x00000600ULL
#define QUP_TOUCH_SPI_IRQ         0x62

/* Synaptics S3330 (DTB: synaptics_dsi_i2c@20). */
#define TOUCH_I2C_SLAVE_ADDR      0x20
#define TOUCH_I2C_UB_ADDR         0x2C   /* synaptics,ub-i2c-addr       */
#define TOUCH_RESET_GPIO          66     /* synaptics,reset-gpio        */
#define TOUCH_IRQ_GPIO            67     /* synaptics,irq-gpio          */
#define TOUCH_RESET_DELAY_MS      200    /* synaptics,reset-delay-ms    */
#define TOUCH_RESET_ACTIVE_MS     5
#define TOUCH_POWER_DELAY_MS      5

/* ---- TLMM (DTB: pinctrl@03000000, qcom,sdm660-pinctrl) ------------- */
#define TLMM_BASE                 0x03000000ULL
/*
 * TLMM v3 family (msm8998/sdm660) places one 0x1000 block per GPIO:
 * CFG at +0x000, INTR_CFG at +0x008, INTR_STATUS at +0x00C.
 * This layout still needs confirmation on jason before enabling touch.
 */
#define TLMM_GPIO_STRIDE          0x1000U
#define TLMM_GPIO_CFG(n)          (TLMM_BASE + (UINTN)(n) * TLMM_GPIO_STRIDE + 0x000)
#define TLMM_GPIO_INTR_CFG(n)     (TLMM_BASE + (UINTN)(n) * TLMM_GPIO_STRIDE + 0x008)
#define TLMM_GPIO_INTR_STATUS(n)  (TLMM_BASE + (UINTN)(n) * TLMM_GPIO_STRIDE + 0x00C)
#define TLMM_CFG_FUNC_MASK        0x0000000FU
#define TLMM_CFG_FUNC_GPIO        0x00000003U  /* SDM660 "GPIO" function */
#define TLMM_CFG_DRV_8MA          (0x3U << 6)
#define TLMM_CFG_PULL_UP          (0x3U << 8)
#define TLMM_CFG_PULL_DOWN        (0x1U << 8)

/* ---- USB3 / DWC3 (DTB: ssusb@a800000, dwc3@a800000) ---------------- */
#define DWC3_BASE                 0x0A800000ULL
#define DWC3_SIZE                 0x0000C8D0ULL
#define DWC3_XHCI_SIZE            0x0000C100ULL  /* xHCI register block   */
#define DWC3_GLOBALS_OFFSET       0x0000C100ULL  /* GCTL/GSTS/GUSB2PHYCFG */
#define DWC3_GCTL                 0x0000C110U
#define DWC3_GCTL_CORESOFTRESET   BIT0
#define DWC3_GUSB2PHYCFG(n)       (0x0000C200U + (UINTN)(n) * 0x04U)
#define DWC3_GUSB3PIPECTL(n)      (0x0000C2C0U + (UINTN)(n) * 0x04U)
#define DWC3_SPI_IRQ              0x83

/* Second USB controller is disabled in the DTB and must stay untouched. */
#define DWC3_SECONDARY_BASE       0x0C200000ULL

/* ---- UART console (DTB: serial@0c170000) --------------------------- */
#define UART_CONSOLE_BASE         0x0C170000ULL
#define UART_CONSOLE_SIZE         0x00001000ULL

/* ---- SPMI arbiter (DTB: qcom,spmi@800f000) ------------------------- */
#define SPMI_CORE_BASE            0x0800F000ULL
#define SPMI_CORE_SIZE            0x00001000ULL
#define SPMI_CHNLS_BASE           0x08400000ULL
#define SPMI_OBSRVR_BASE          0x09400000ULL
#define SPMI_INTR_BASE            0x0A400000ULL
#define SPMI_CNFG_BASE            0x0800A000ULL

/*
 * Arbiter programming model verified against drivers/spmi/spmi-pmic-arb.c
 * (v2 ops) and the DTB property qcom,ee = <0>.
 *
 *   per-channel window  = 0x1000 * ee + 0x8000 * apid
 *   command word        = (opc << 27) | ((addr & 0xFF) << 4) | (len - 1)
 *   PPID                = (sid << 8) | ((addr >> 8) & 0xFF)
 *   PPID -> APID table  = core + 0x800 + 4 * apid, PPID in bits 11:0 of
 *                         (regval >> 8)
 */
#define SPMI_EE                   0x00000000U
#define SPMI_EE_STRIDE            0x00001000U
#define SPMI_APID_STRIDE          0x00008000U
#define SPMI_APID_MAP_BASE        0x00000800U
#define SPMI_APID_MAX             512U
#define SPMI_REG_CMD              0x00U
#define SPMI_REG_CONFIG           0x04U
#define SPMI_REG_STATUS           0x08U
#define SPMI_REG_WDATA0           0x10U
#define SPMI_REG_WDATA1           0x14U
#define SPMI_REG_RDATA0           0x18U
#define SPMI_REG_RDATA1           0x1CU
#define SPMI_STATUS_DONE          BIT0
#define SPMI_STATUS_FAILURE       BIT1
#define SPMI_STATUS_DENIED        BIT2
#define SPMI_STATUS_DROPPED       BIT3
#define SPMI_OPC_EXT_WRITEL       0U
#define SPMI_OPC_EXT_READL        1U
#define SPMI_CMD_OPCODE(opc, addr, len) \
  (((UINT32)(opc) << 27) | (((UINT32)(addr) & 0xFFU) << 4) | ((UINT32)((len) - 1U) & 0x7U))

/* ---- PMIC slave ids (DTB: qcom,pm660@0/1, qcom,pm660l@2/3) ---------- */
#define PM660_SID                 0x00U
#define PM660L_SID                0x02U

/* PON (DTB: qcom,power-on@800). RT_STS at +0x10, see qpnp-power-on.c. */
#define PMIC_PON_BASE             0x0800U
#define PMIC_PON_RT_STS           0x0810U
#define PMIC_PON_KPDPWR_BIT       BIT0   /* pon_1: KEY_POWER    */
#define PMIC_PON_RESIN_BIT        BIT1   /* pon_2: KEY_VOLUMEDOWN */

/* PM660L GPIO (DTB: pm660l-gpio) - one 0x100 block per pin, RT_STS at 0x10 */
#define PM660L_GPIO_BASE          0x0C000U
#define PM660L_GPIO_STRIDE        0x0100U
#define PM660L_GPIO_RT_STS        0x0010U
#define PM660L_GPIO_VOL_UP_PIN    7U     /* DTB gpios = <&pm660l_gpios 7 1> */

/* FG gen3 (DTB: qcom,fg-batt-soc@4000, compatible qcom,fg-gen3) ------ */
#define FG_SOC_BASE               0x4000U
#define FG_SOC_MONOTONIC_SOC      0x0009U  /* 2 bytes, both bytes must match */
#define FG_SOC_RAW_FULL           0xFFU

/* ---- reserved-memory regions that constrain any UEFI load address --- */
#define JASON_RESERVED_WLAN_GUARD 0x85600000ULL
#define JASON_RESERVED_SPLASH     0x9D400000ULL
#define JASON_RESERVED_DFPS       0x9F7FF000ULL
