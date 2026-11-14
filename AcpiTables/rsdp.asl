/*
 * RSDP — Root System Description Pointer for SDM660 / Xiaomi Note3
 *
 * This ASL fragment generates a minimal RSDP that points to an RSDT
 * containing only our custom SSDTs. Windows ARM64 expects the RSDP to
 * be found in either:
 *   - EBDA (Extended BIOS Data Area), or
 *   - first 1K of EBIOS memory (0x000E0000–0x000FFFFF), or
 *   - a UEFI ACPI table protocol registration during DXE boot.
 *
 * The third path is what we use here — the UEFI firmware registers the
 * RSDP address with Windows via the EFI_ACPI_TABLE_PROTOCOL at PEI → DXE.
 *
 * Compiled: iasl -tc rsdp.asl → rsdp.dat (RSDP binary blob)
 */

DefinitionBlock ("rsdp.dat", "RSDP", 2, "JASON", "JN_RSDP", 0x00001001)
{
    /* RSDP descriptor structure — 20 bytes (v1.0) + 36 bytes (v2.0 ext) = 56 total */
    // NOTE: this file intentionally left as a documentation placeholder.
    // Real RSDP generation happens through EDK II's UefiAcpiTableLib; see
    // AcpiTables.inf for the corresponding .inf module entry point.
}
