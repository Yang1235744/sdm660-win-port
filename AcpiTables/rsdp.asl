/*
 * RSDP integration note — do not compile or install this file as an ACPI table.
 *
 * RSDP is a root pointer, not an SSDT and not an XSDT child table. The EDK II
 * ACPI table implementation creates and exposes RSDP/XSDT after DXE drivers
 * install MADT and SSDT tables through EFI_ACPI_TABLE_PROTOCOL.
 *
 * This .asl suffix is retained only for compatibility with the original tree;
 * AcpiTables.inf and gcc_tables.py deliberately exclude it from AML builds.
 */
