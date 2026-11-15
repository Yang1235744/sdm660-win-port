#ifndef JASON_MADT_GENERATOR_H_
#define JASON_MADT_GENERATOR_H_

#include <Uefi.h>
#include <IndustryStandard/Acpi64.h>

EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *
EFIAPI
MadtGenerate (
  OUT UINTN *OutSize
  );

#endif
