#include <Uefi.h>
#include <IndustryStandard/Acpi64.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include "MadtGenerator.h"

#define JASON_CPU_COUNT    8
#define JASON_GICR_STRIDE  0x20000

STATIC_ASSERT (sizeof (EFI_ACPI_6_4_GIC_STRUCTURE) == 80, "GICC must be 80 bytes");
STATIC_ASSERT (sizeof (EFI_ACPI_6_4_GIC_DISTRIBUTOR_STRUCTURE) == 24, "GICD must be 24 bytes");
STATIC_ASSERT (sizeof (EFI_ACPI_6_4_GICR_STRUCTURE) == 16, "GICR must be 16 bytes");

/* Source: jason DTB /cpus; each cpu@<MPIDR> node has a matching reg value. */
STATIC CONST UINT64 mJasonMpidr[JASON_CPU_COUNT] = {
  0x000, 0x001, 0x002, 0x003,
  0x100, 0x101, 0x102, 0x103
};

STATIC
UINT8
AcpiChecksum (
  IN CONST UINT8 *Buffer,
  IN UINTN       Length
  )
{
  UINT8 Sum;
  UINTN Index;

  Sum = 0;
  for (Index = 0; Index < Length; Index++) {
    Sum = (UINT8)(Sum + Buffer[Index]);
  }

  return (UINT8)(0U - Sum);
}

/**
  Generate an ACPI 6.4 MADT for jason.

  Source: jason DTB /soc/interrupt-controller@17a00000. The node identifies
  arm,gic-v3, one 64 KiB GICD window and one 1 MiB GICR window with 128 KiB
  per-CPU stride. GICv3 CPU interfaces use System Registers, not a GICv2 MMIO
  window. The GICR subtable describes the contiguous Redistributor range, so
  the per-GICC GICRBaseAddress fields remain zero.

  @param[out] OutSize  Size of the returned table, or zero on failure.

  @return A pool-allocated MADT. The caller must free it with FreePool().
  @retval NULL  A required GIC address is not configured, or allocation failed.
**/
EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *
EFIAPI
MadtGenerate (
  OUT UINTN *OutSize
  )
{
  EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *Madt;
  EFI_ACPI_6_4_GIC_DISTRIBUTOR_STRUCTURE              *Gicd;
  EFI_ACPI_6_4_GICR_STRUCTURE                         *Gicr;
  EFI_ACPI_6_4_GIC_STRUCTURE                          *Gicc;
  UINT64                                              GicdBase;
  UINT64                                              GicrBase;
  UINT64                                              GicrLength;
  UINTN                                               TotalSize;
  UINTN                                               Index;
  UINT8                                               *Cursor;

  if (OutSize != NULL) {
    *OutSize = 0;
  }

  GicdBase   = PcdGet64 (PcdGicDistributorBase);
  GicrBase   = PcdGet64 (PcdGicRedistributorBase);
  GicrLength = PcdGet64 (PcdGicRedistributorLength);
  if ((GicdBase == 0) || (GicrBase == 0) ||
      (GicrLength < (JASON_CPU_COUNT * JASON_GICR_STRIDE)) ||
      (GicrLength > MAX_UINT32)) {
    return NULL;
  }

  TotalSize = sizeof (*Madt) + sizeof (*Gicd) + sizeof (*Gicr) +
              (JASON_CPU_COUNT * sizeof (*Gicc));
  Madt = AllocateZeroPool (TotalSize);
  if (Madt == NULL) {
    return NULL;
  }

  CopyMem (&Madt->Header.Signature, "APIC", 4);
  Madt->Header.Length          = (UINT32)TotalSize;
  Madt->Header.Revision        = EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION;
  CopyMem (Madt->Header.OemId, "QCOM  ", 6);
  CopyMem (&Madt->Header.OemTableId, "JASON   ", 8);
  Madt->Header.OemRevision     = 0x00001001;
  Madt->Header.CreatorId       = SIGNATURE_32 ('J', 'A', 'S', 'N');
  Madt->Header.CreatorRevision = 0x00000001;
  Madt->LocalApicAddress       = 0;
  Madt->Flags                  = 0;

  Cursor = (UINT8 *)(Madt + 1);

  Gicd = (EFI_ACPI_6_4_GIC_DISTRIBUTOR_STRUCTURE *)Cursor;
  Gicd->Type                = EFI_ACPI_6_4_GICD;
  Gicd->Length              = sizeof (*Gicd);
  Gicd->GicId               = 0;
  Gicd->PhysicalBaseAddress = GicdBase;
  Gicd->SystemVectorBase    = 0;
  Gicd->GicVersion          = EFI_ACPI_6_4_GIC_V3;
  Cursor += sizeof (*Gicd);

  Gicr = (EFI_ACPI_6_4_GICR_STRUCTURE *)Cursor;
  Gicr->Type                      = EFI_ACPI_6_4_GICR;
  Gicr->Length                    = sizeof (*Gicr);
  Gicr->DiscoveryRangeBaseAddress = GicrBase;
  Gicr->DiscoveryRangeLength      = (UINT32)GicrLength;
  Cursor += sizeof (*Gicr);

  for (Index = 0; Index < JASON_CPU_COUNT; Index++) {
    Gicc = (EFI_ACPI_6_4_GIC_STRUCTURE *)Cursor;
    Gicc->Type                      = EFI_ACPI_6_4_GIC;
    Gicc->Length                    = sizeof (*Gicc);
    Gicc->CPUInterfaceNumber        = (UINT32)Index;
    Gicc->AcpiProcessorUid          = (UINT32)Index;
    Gicc->Flags                     = EFI_ACPI_6_4_GIC_ENABLED;
    Gicc->PhysicalBaseAddress       = 0;
    Gicc->GICRBaseAddress           = 0;
    Gicc->MPIDR                     = mJasonMpidr[Index];
    /* No virtual GIC interface is described without hardware evidence. */
    Gicc->VGICMaintenanceInterrupt  = 0;
    Cursor += sizeof (*Gicc);
  }

  Madt->Header.Checksum = AcpiChecksum ((CONST UINT8 *)Madt, TotalSize);
  if (OutSize != NULL) {
    *OutSize = TotalSize;
  }

  return Madt;
}
