# SDM660 WinPort — Architecture Notes

## 1. Target and current state

The target is Xiaomi Mi Note 3 (`jason`, SDM660) with an AArch64 UEFI/ACPI environment capable of eventually loading Windows ARM64. The repository currently contains DXE driver research modules and an FV payload container, not a complete firmware boot chain.

The following stages are **not implemented or verified**: SEC/PEI, DXE Core, UEFI Boot Manager, a signed Android boot-chain handoff, storage boot of `bootmgfw.efi`, and Windows hardware enablement.

## 2. Intended boot research flow

```text
Qualcomm PBL
  → SBL/XBL
    → ABL/aboot
      → Android boot.img / a future verified handoff mechanism
        → UEFI firmware environment
          → DXE drivers + ACPI installation
            → UEFI boot application / bootmgfw.efi
```

The exact jason signature and AVB requirements are unknown. See `hardware-notes.md`; no DTB-overlay or aboot patch in this tree should be treated as a working handoff.

## 3. Memory and FDF

The old assumption of RAM at `0x80000000` and a payload near `0xA0000000` has not been validated against the device's reserved-memory map. `Jason.fdf` uses that range only to describe a research FD container. It is not permission to load or flash at that address.

Before any runtime experiment, obtain the matching DTB, reserved-memory nodes, `/proc/iomem`, bootloader load conventions and a recovery path.

## 4. ACPI strategy

```text
EDK2 ACPI implementation
  ├── creates RSDP / XSDT
  ├── installs MADT from AcpiTables/madt_gen.c
  └── must install validated SSDTs through a future completed path

MADT (ACPI 6.4, Revision 5)
  ├── GICD (Type 0x0C), base from verified PCD
  └── 8 × GICC (Type 0x0B), standard 80-byte entries
```

GICD and GICC PCDs default to zero. In that state the MADT driver returns `EFI_NOT_READY` and installs nothing. No address such as the old `0xB1000000`/`0xB1100000` values is accepted without jason-specific evidence.

`ssdt_cpu.asl` provides processor objects whose `_UID` values match the MADT UIDs. `ssdt_periph.asl` is intentionally empty until peripheral IDs, addresses and interrupt resources are known. RSDP is not authored as an SSDT.

## 5. Driver loading policy

All six hardware driver entry points currently return `EFI_UNSUPPORTED`. This prevents automatic DXE dispatch from touching unverified MMIO. The remaining functions are research references, not validated driver implementations.

```text
ClocksGpcd      requires verified GCC binding, clock tree and register offsets
PmicRegulator   requires actual SPMI/PMIC semantics and safe power sequencing
StorageUfs      requires verified HC node, clocks/reset/PHY and real queries
UsbHost         requires controller role, PHY, clocks and correct register map
TouchI2c        requires QUP instance, touch compatible and pinctrl
WifiQca         requires PCIe enumeration, BDF/BAR and firmware protocol
```

## 6. Highest-priority inputs

1. Matching jason `boot.img`, extracted DTB/DTS and image hash.
2. Rooted `/proc/iomem`, `/proc/interrupts`, kernel log and live device tree.
3. GIC and CPU topology evidence for MADT.
4. UFS and PCIe data before storage/Wi-Fi experiments.
5. Signature-chain and AVB documentation before any write operation.

## References

- EDK II: https://github.com/tianocore/edk2
- ACPI specifications: https://uefi.org/specifications
- Linux SDM660/jason DTS and Qualcomm drivers corresponding to the device's kernel version
