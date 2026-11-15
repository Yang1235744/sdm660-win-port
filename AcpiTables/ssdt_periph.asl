/*
 * Peripheral SSDT for jason (SDM660, Mi Note 3) aimed at Windows 10 ARM64.
 *
 * Resource values below come from the matching boot.img DTB and are recorded in
 * Docs/hardware-notes.md. Two different kinds of device are published here:
 *
 *   1. Devices with an ACPI-defined identifier that Windows already knows:
 *        XHC0  _HID = PNP0D10  (USB xHCI host controller)
 *      Windows 10 ARM64 binds its inbox USB stack to this, so USB ports and
 *      mass storage work without any vendor driver.
 *
 *   2. Devices that have no public Windows driver yet:
 *        WLAN  _HID = JAS0001  (ICNSS / WCN3990 WLAN)
 *        UFS0  _HID = QCOM24A5 (verified Qualcomm UFS binding)
 *      JAS0001 is a Jason placeholder, not a registered vendor ID. It makes
 *      the WLAN hardware visible at a known ACPI path with the right memory
 *      window, which is what a vendor INF needs in order to bind; until such
 *      an INF exists the device shows up without a driver. note that the
 *      edk2-msm WCN3990 node (Platform/Google/msm8998/AcpiTables/walleye/
 *      wcnss_wlan.asl) also carries no _HID, so no vendor HID is known.
 *      QCOM24A5 is different: it is the identifier Qualcomm UFS binds to on
 *      Windows ARM64, see the device below.
 *
 * Verified resources (DTB nodes under /soc):
 *   ICNSS / WLAN   qcom,icnss@18800000   0x18800000 / 0x00800000, SPI 413..425
 *   USB3 wrapper   ssusb@a800000         0x0A800000 / 0x00C8D0,  SPI 131
 *   UFS host       ufshc@1da4000         0x01DA4000 / 0x00003000, SPI 265
 *   UFS PHY        ufsphy@1da7000        0x01DA7000 / 0x00000DB8
 *   UFS ICE        ufsice@1db0000        0x01DB0000 / 0x00008000
 *
 * MADT, MCFG and the other fixed tables are not authored here.
 */

DefinitionBlock ("ssdt_periph.aml", "SSDT", 2, "JASON", "PERI", 0x00000002)
{
    Scope (\_SB)
    {
        /*
         * USB xHCI. Only the xHCI register block is described (the DWC3 core
         * starts at 0xC100), which is what the host controller driver needs.
         *
         * The interrupt is the GIC SPI 131 from the DTB, expressed as the ACPI
         * GSI 163 (SPI + 32). Trigger mode is taken from the GIC SPI default;
         * confirm level/active-high against /proc/interrupts on the device.
         */
        Device (XHC0)
        {
            Name (_HID, "PNP0D10")
            Name (_UID, 0)
            Name (_STA, 0x0F)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x0A800000, 0x0000C100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 163 }
            })
        }

        /*
         * WLAN (ICNSS / WCN3990). No PCIe host exists on jason: the chip is
         * attached through the ICNSS wrapper below, so a PCIe style binding
         * cannot work here.
         */
        Device (WLAN)
        {
            Name (_HID, "JAS0001")
            Name (_UID, 0)
            Name (_STA, 0x0F)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x18800000, 0x00800000)
            })
        }

        /*
         * UFS host controller. The Windows hardware id is produced by _CID,
         * so both strings are required: _HID = QCOM24A5 plus
         * _CID = ACPI\QCOM24A5. This is the identifier used by the Qualcomm
         * UFS ACPI binding on SDM845/SDM660 class platforms (edk2-msm
         * Silicon/Qualcomm/sdm845/AcpiTables/ufs.asl, and the same value in
         * drivers/ufs/host/ufs-qcom.c).
         *
         * Only the memory windows and the SPI are Jason specific: host
         * 0x01DA4000/0x3000, PHY 0x01DA7000/0xDB8, ICE 0x01DB0000/0x8000,
         * SPI 265 (ACPI GSI 297 = SPI + 32).
         */
        Device (UFS0)
        {
            Name (_HID, "QCOM24A5")
            Name (_CID, "ACPI\\QCOM24A5")
            Name (_UID, 0)
            Name (_STA, 0x0F)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x01DA4000, 0x00003000)
                Memory32Fixed (ReadWrite, 0x01DA7000, 0x00000DB8)
                Memory32Fixed (ReadWrite, 0x01DB0000, 0x00008000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 297 }
            })
        }
    }
}
