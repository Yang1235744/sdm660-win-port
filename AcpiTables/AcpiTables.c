#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/AcpiTable.h>

#include "MadtGenerator.h"

EFI_STATUS
EFIAPI
AcpiTablesEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_ACPI_TABLE_PROTOCOL                         *AcpiTable;
  EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *Madt;
  EFI_STATUS                                      Status;
  UINTN                                           TableKey;
  UINTN                                           MadtSize;

  Madt = MadtGenerate (&MadtSize);
  if (Madt == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "JASON ACPI: GIC addresses are unconfigured or allocation failed; MADT not installed\n"
      ));
    return EFI_NOT_READY;
  }

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    FreePool (Madt);
    return Status;
  }

  TableKey = 0;
  Status = AcpiTable->InstallAcpiTable (AcpiTable, Madt, MadtSize, &TableKey);
  FreePool (Madt);

  return Status;
}
