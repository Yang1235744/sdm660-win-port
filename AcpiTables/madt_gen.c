/*
 * madt_gen.c — 手动组装 SDM660 MADT（Multiple APIC Description Table）
 *
 * Windows ARM64 的 ACPI 解析器会从 RSDP → RSDT/XSDT 一路往下找 MADT，
 * 然后从 MADT 中找到所有 GICC sub-entry 建立 CPU-to-IRQ 映射。
 *
 * 本文件是 EDK II 风格的 C 实现，生成完毕后通过 InstallAcpiTable() 注册。
 * 可以直接在 UefiPlatform/Platform.c 中调用 MadtGenerate()。
 *
 * MADT Header 布局 (ACPI 6.4 §5.2.12.2):
 *   Offset  Size  Field
 *   ------  ----  -----
 *   0x00    4     Signature = "APIC"
 *   0x04    4     Length (总长度)
 *   0x08    1     Revision
 *   0x09    1     Checksum
 *   0x0A    6     OemId ("QCOMSDM6")
 *   0x10    6     OemTableId ("JASONMAD")
 *   0x16    4     OemRevision (0x00001001)
 *   0x1A    4     CreatorId ("MSFT")
 *   0x1E    4     CreatorVersion (0x00000001)
 *   0x22    1     Local apic address   (GICC address, AArch64 uses this field differently)
 *   0x23    4     Flags (bit 0 = PCAT compatible)
 *   0x27    ...  SubEntries[]          ← variable length
 *
 * GICC Sub-entry 布局:
 *   Type    Len   Reserved  CPU Interface Address (64-bit)  CPU Interface Number  MPIDR  Flags  Local IRQ  PMU IRQ
 *   0x01    0x30  2 bytes   8 bytes                         4 bytes               4    1      4          4
 */

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#define MADT_SIGNATURE    0x43495041          /* "APIC" in little-endian */
#define MADT_MAX_GICC     8                /* jason has 8 cores        */

#pragma pack(push, 1)

typedef struct {
    UINT8   Type;             // 0x01 = GICC
    UINT8   Length;           // always 0x30 for GICC v2
    UINT8   Reserved[2];
    UINT64  Address;          // GICC register base (64-bit)
    UINT32  ArmMpId;          // MPIDR value for this CPU
    UINT32  CpuInterfaceNumber;
    UINT32  Flags;            // BIT(0)=enabled, BIT(1)=online-capable
    UINT32  LocalGicInterrupt; // GIC virtual interrupt number
    UINT32  LocalPmuInterrupt; // PMU virtual interrupt
} EFI_ACPI_6_4_MADT_GICC_STRUCTURE;

typedef struct {
    EFI_ACPI_DESCRIPTION_HEADER Header;
    UINT32  LocalApicAddress;     // reserved on ARM, kept for compatibility
    UINT32  Flags;
    // followed by variable-length entries
} EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE;

#pragma pack(pop)

// GICC flags bit masks
#define ACPI_MADT_ENABLED           (1 << 0)
#define ACPI_MADTOnline_CAPABLE     (1 << 1)

// GICC MPIDR values for SDM660 (from dtsi cpu maps)
// MPIDR layout: [31:24]=cluster, [23:16]=CPU, [15:8]=res, [7:0]=res
// Cluster 0 = little (A53), Cluster 1 = big (A73)
STATIC const UINT32 SDM660_MPIDR[] = {
    0x00000000,  // CPU0 A53 cluster 0
    0x00000001,  // CPU1 A53
    0x00000002,  // CPU2 A53
    0x00000003,  // CPU3 A53
    0x01000000,  // CPU4 A73 cluster 1
    0x01000001,  // CPU5 A73
    0x01000002,  // CPU6 A73
    0x01000003,  // CPU7 A73
};

/**
 * MadtGenerate — 在运行时动态构造完整的 MADT 表并返回指向它的指针。
 *
 * 返回使用 EfiAllocateRuntimePool 分配的内存，调用方负责释放。
 * 失败时返回 NULL，*OutSize 保持未定义。
 */
EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE *
EFIAPI
MadtGenerate (
  OUT UINTN *OutSize   // optional — receives total allocated size
  )
{
  UINTN EntrySize    = sizeof(EFI_ACPI_6_4_MADT_GICC_STRUCTURE);
  UINTN SubEntryCount = MADT_MAX_GICC;
  UINTN TotalSize     = sizeof(EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE)
                      + (EntrySize * SubEntryCount);
  UINTN i;

  EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE *MadT =
    (EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE *)
    AllocateRuntimePool (TotalSize);
  if (!MadT)
    return NULL;
  ZeroMem (MadT, TotalSize);

  // Fill header
  CopyMem (&MadT->Header.Signature, "APIC", 4);
  MadT->Header.Length         = (UINT32)TotalSize;
  MadT->Header.Revision       = 0x03;      // ACPI 6.3
  CopyMem (MadT->Header.OemId,      "QCOMSDM6", 6);
  CopyMem (MadT->Header.OemTableId, "JASONMAD", 6);
  MadT->Header.OemRevision   = 0x00001001;
  MadT->Header.CreatorId     = SIGNATURE_32('M','S','F','T');
  MadT->Header.CreatorVersion = 0x00000001;

  MadT->LocalApicAddress = 0;               // reserved on ARM
  MadT->Flags = 0;                          // no PCAT compatibility

  // Fill GICC entries
  for (i = 0; i < SubEntryCount; ++i) {
    EFI_ACPI_6_4_MADT_GICC_STRUCTURE *Gicc =
      (EFI_ACPI_6_4_MADT_GICC_STRUCTURE *)((UINT8 *)MadT + sizeof(*MadT) + i * EntrySize);

    Gicc->Type        = 0x01;            // GICC subtype
    Gicc->Length      = EntrySize;
    Gicc->Reserved[0] = 0;
    Gicc->Reserved[1] = 0;
    Gicc->Address     = 0x00B1100000ULL + (i * 0x2000);  // per-CPU GICC dist offset
    Gicc->ArmMpId     = SDM660_MPIDR[i];
    Gicc->CpuInterfaceNumber = i;
    Gicc->Flags     = ACPI_MADT_ENABLED;       // all cores enabled at boot
    Gicc->LocalGicInterrupt = 0x00C + i;       // SPI range starting at 12
    Gicc->LocalPmuInterrupt = 0x00D + i;       // PMU interrupt (per-core)
  }

  // Compute checksum (required by ACPI spec)
  MadT->Header.Checksum = 0;
  {
    UINT8 *Bytes = (UINT8 *)MadT;
    UINT8 Sum = 0;
    for (UINTN k = 0; k < TotalSize; ++k)
      Sum += Bytes[k];
    MadT->Header.Checksum = (UINT8)(0 - Sum);
  }

  if (OutSize)
    *OutSize = TotalSize;
  return MadT;
}
