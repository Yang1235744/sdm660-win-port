/*
 * ssdt_periph.asl — Peripheral mapping SSDT for SDM660 (UART, SPI, GPIO, etc.)
 *
 * Windows ARM64 uses the MADT / MCFG tables to discover serial ports, PCIe,
 * and other resources. This SSDT fills in the remaining gaps not covered by
 * the standard MADT:
 *
 *   - UART2 (BLSP1)          → COM port assignment
 *   - QUP SPI1 (WiFi interface)
 *   - QUP I2C1 (touch panel)
 *   - TLMM GPIO controller
 *   - Watchdog timer
 *
 * Compile with:  iasl -tc ssdt_periph.asl → ssdt_periph.aml
 */

DefinitionBlock ("ssdt_periph.aml", "SSDT", 2, "JASON", "PERI", 0x00000001)
{
    Scope (\_SB)
    {
        /* ── UART2: Bluetooth LE / console dual-use ─────────────── */
        Device (UAR2)
        {
            Name (_HID, "INT33C6")   /* Intel serial UART compatible ID */
            Name (_CID, "PNP0500")   /* Standard 16550-compatible fallback */
            Name (_UID, "BLSP1_UART2")
            Name (_ADR, 0x13C00000)  /* Physical MMIO base */

            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadOnly, 0x13C00000, 0x1000)
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x3C }
                    /* IRQ 60 = BLSP1_UART2_GSI; from dts pinctrl-0 mapping */
            })

            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        /* ── Touch I2C (QUP #2) ─────────────────────────────────── */
        Device (TI2C)
        {
            Name (_HID, "QCOM8021")   /* Qualcomm QUP I2C vendor ID */
            Name (_UID, "QUP_I2C1")
            Name (_ADR, 0x164C0000)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x164C0000, 0x1000)
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x6D }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        /* ── WiFi SPI (QUP #1) — used for Wi-Fi AT command interface */
        Device (TSPI)
        {
            Name (_HID, "QCOM8020")   /* Qualcomm QUP SPI vendor ID */
            Name (_UID, "QUP_SPI1")
            Name (_ADR, 0x164B0000)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x164B0000, 0x1000)
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x6C }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        /* ── GPIO controller (TLMM) ─────────────────────────────── */
        Device (GPIO)
        {
            Name (_HID, "INT33FF")    /* Generic GPIO parent */
            Name (_CID, "MSFT8001")   /* Microsoft GPIO compatible */
            Name (_ADR, 0x10000000)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10000000, 0x100000)
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x2D }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        /* ── Watchdog Timer ─────────────────────────────────────── */
        Device (WDG)
        {
            Name (_HID, "INT0C00")    /* WHEA watchdog device */
            Name (_UID, "WDT")
            Name (_ADR, 0x10500000)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10500000, 0x1000)
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x3E }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
    }
}
