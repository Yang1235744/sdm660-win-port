/*
 * ssdt_madt_gicc.asl — MADT GICC ACPI SSDT (补充完整的中断控制器描述)
 *
 * Windows ARM64 启动时首先需要 MADT (Multiple APIC Description Table)
 * 中的 GICC (Generic CPU Interface) 条目来建立每核 IRQ 路由。
 *
 * 注意：MADT 本身不是标准 SSDT，它是独立表，在 RSDT/XSDT 中与 SSDT 并列。
 * 本文件生成的是 MADT 表中各 Sub-Entry 的二进制表示模板，
 * 实际打包进 MADT 的方式是在 EDK II 的 AcpiTable.inf 里通过
 * EFI_ACPI_TABLE_PROTOCOL→InstallAcpiTable() 一次性提交整张 MADT。
 *
 * 硬件参考：
 *   - SDM660 TRM §25 "Generic Interrupt Controller (GIC)"
 *   - gic_dist_base = 0x00B1000000  (64-bit, 超出 32-bit MMIO range)
 *   - gic_cpu_base  = 0x00B1100000
 *   - gic_hyp_base  = 0x00B1200000
 *   - gic_vcpu_base = 0x00B1300000
 *
 * 每条 GICC sub-entry 对应一个 CPU 逻辑核（ARM MPS1/MPS2 兼容格式）。
 */

DefinitionBlock ("ssdt_madt_gicc.aml", "SSDT", 2, "JASON", "GICC", 0x00000001)
{
    /* 注意：真正的 MADT 不在 SSDT 中，而是在 RSDT 顶层单独一张表。
       此 SSDT 仅作为演示 —— 展示每个 GICC 条目的字段布局。
       生产代码需在 AcpiTables/AcpiGen.c 里手动组装 MADT header
       + GICC entries 数组，再整体注册到 RSDT。 */

    Scope (\_SB)
    {
        /* 示例：CPU0 的 GICC 条目结构（二进制布局）
           Subtype   Length  Addr[64bit]  Id  Flags  Local_IRQ  PMUIrq
           -------------------------------------------------------
           0x01(GICC) 0x30   0xB1100000  0x0  0x01   0x00C    0x00D
        */
        Device (GICC0)
        {
            Name (_HID, "ACPI000F")   /* Generic CPU Interface */
            Name (_UID, 0x00)
            Name (_CRS, ResourceTemplate ()
            {
                /* GIC distributor base (32-bit truncated representation)
                   Real address is 0x00B1000000 (64-bit); we split high/low */
                Memory64Fixed (ReadWrite, 0x00B1000000ULL, 0x00001000)
                Memory64Fixed (ReadWrite, 0x00B1100000ULL, 0x00001000)
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x00C }
                Interrupt (Level, ActiveHigh, Exclusive, Shared) { 0x00D }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        /* 其余 7 个 CPU 的 GICC 条目结构相同，仅 _UID 不同 */
        Device (GICC1) { Name (_HID, "ACPI000F"); Name (_UID, 0x01); }
        Device (GICC2) { Name (_HID, "ACPI000F"); Name (_UID, 0x02); }
        Device (GICC3) { Name (_HID, "ACPI000F"); Name (_UID, 0x03); }
        Device (GICC4) { Name (_HID, "ACPI000F"); Name (_UID, 0x04); }
        Device (GICC5) { Name (_HID, "ACPI000F"); Name (_UID, 0x05); }
        Device (GICC6) { Name (_HID, "ACPI000F"); Name (_UID, 0x06); }
        Device (GICC7) { Name (_HID, "ACPI000F"); Name (_UID, 0x07); }
    }
}
