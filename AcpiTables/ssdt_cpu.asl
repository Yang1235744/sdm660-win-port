/*
 * ssdt_cpu.asl — CPU topology SSDT for SDM660 (8 cores: 4×A73 big + 4×A53 little)
 *
 * Windows ARM64 needs to know about all logical processors early in boot
 * so it can assign interrupt vectors and NUMA nodes correctly.
 *
 * Architecture note: SDM660 uses AArch64 GICv2 (Generic Interrupt Controller).
 * The MADT (Multiple APIC Description Table) entry must reference our
 * SSDT CPU entries. MADT itself lives outside this repo — see
 * Docs/madt-layout.md for the expected layout.
 *
 * Each _CRS (Current Resource Settings) block defines:
 *   - Memory-mapped base address for the CPU's private peripherals
 *   - SPI count for GIC peripheral interrupts
 *
 * Reference:
 *   - ACPI Specification 6.4 §5.2.12 "Processor Local SAPIC"
 *   - linux/dt/qcom,sdm660.dtsi node &cpu0 … &cpu7
 */

DefinitionBlock ("ssdt_cpu.aml", "SSDT", 2, "JASON", "CPUS", 0x00000001)
{
    Scope (\_SB)
    {
        /* Big cluster (A73) — CPUs 0..3 */
        Processor (_CPU, 0x00, 0x00004210, 0x06) // A73, affinity 0
        {
            Name (_HID, "ACPID001")  /* HAL Processor Interface */
            Name (_UID, 0x00)
            Name (_STA, 0x0F)         /* Present, Enabled, Bootable */
            Name (_PRW, Package() { 0x00, 0x00 })  /* Power Resources — wakeup */
            Method (_ON, 0, NotSerialized)
            {
                /* Bring core out of reset: set bit [31] in SCM call */
                // SCM call index 0x41 = set CPU power state ON
                Debug("CPU0: powered on\n")
            }
            Method (_OFF, 0, NotSerialized)
            {
                Debug("CPU0: powering off\n")
            }
            Name (_PCT, Package()
            {
                0x01B80000,  // PMCNTRL register address (GCC_BASE)
                0x01B80004,  // PMFLAGS register address
            })
            Name (_TPC, Package()
            {
                0x00,       /* reserved */
                0x00,       /* reserved */
                0x00,       /* reserved */
            })
            Name (_PSS, Package(0x04)     /* Performance States: 4 levels */
            {
                Package() { 1804000, 100, 100, 0 },   /* max freq 1.8 GHz */
                Package() { 1363000, 90,  90,  0 },   /* mid-high           */
                Package() { 1075000, 80,  80,  0 },   /* mid-low            */
                Package() {  576000, 60,  60,  0 },   /* idle               */
            })
        }

        Processor (_CPU, 0x01, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x01); Name (_STA, 0x0F); }
        Processor (_CPU, 0x02, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x02); Name (_STA, 0x0F); }
        Processor (_CPU, 0x03, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x03); Name (_STA, 0x0F); }

        /* Little cluster (A53) — CPUs 4..7 (running first; SMP bringup later) */
        Processor (_CPU, 0x04, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x04); Name (_STA, 0x0F); }
        Processor (_CPU, 0x05, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x05); Name (_STA, 0x0F); }
        Processor (_CPU, 0x06, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x06); Name (_STA, 0x0F); }
        Processor (_CPU, 0x07, 0x00004210, 0x06)
        { Name (_HID, "ACPID001"); Name (_UID, 0x07); Name (_STA, 0x0F); }
    }
}
