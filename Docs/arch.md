# SDM660 WinPort — Architecture Notes

## 1. Boot Flow Overview

```
   Android bootloader (aboot/fastboot)
        │
        ├─→ Kernel + DTB (Linux)   ← normal path, kept as fallback
        │
        └─→ [OUR INJECTION]         target path for Windows boot
             │
             ├─ 1. UEFI PEI phase boots on CPU0 @ small freq
             ├─ 2. PLL clock tree initialized (GPLL0 → CPU cores)
             ├─ 3. PMIC rails powered up in sequence: core → memory → peripherals
             ├─ 4. ACPI tables installed via EFI_ACPI_TABLE_PROTOCOL
             ├─ 5. Storage controller probed; FAT partition scanned
             ├─ 6. bootmgfw.efi loaded from /EFI/Microsoft/Boot/bootmgfw.efi
             ├─ 7. Control transferred to Windows ARM64 kernel
```

### How to inject our UEFI firmware into the boot chain

The cleanest injection point is **Android's initramfs** — we add a tiny
stub that detects a hardware switch (or a magic file on `/userdata`) and
hands control to a secondary UEFI payload instead of starting the Linux
kernel. Two viable mechanisms:

| Mechanism                       | Effort    | Risk      |
|---------------------------------|-----------|-----------|
| Replace `boot.img` recovery key with our DTB overlay     | medium    | high (brick if wrong) |
| Add a QCOM SPL hook at aboot exit                        | low-medium| low (only triggers when flag set) |
| Build a custom `aboot` from Qualcomm open-source BSP     | high      | lowest (full control)   |

The `Scripts/InjectPayload.py` helper automates the DTB-overlay approach.

---

## 2. Memory Map (SDM660 default layout)

```
Physical Address    Size       Usage
───────────────────────────────────────────────────────
0x0000_0000         256 MB     ROM / boot media
0x8000_0000         4 GB       LPDDR4 main RAM (starts here on jason)
                              ... ...
                              ← UEFI stack allocated from top-down at runtime
                              ← firmware volume blobs placed just below Linux reservation
                              ← ACPI NVS region (~2 MB)
                              ← RSDP / ACPI pointer registers (fixed addr 0x000E0000+)
0xFFFF_FFFF                             end of 32-bit physical address space
```

UEFI firmware must live somewhere inside the LPDDR range. Recommended base:
`0xA000_0000` (below any existing Linux reservation). Verify with `dmesg
| grep -i memblock` on the host device before hard-coding.

---

## 3. ACPI Table Strategy

Windows ARM64 requires the following table hierarchy:

```
RSDP → RSDT/XSDT → MADT, FACP, HPET, MCFG, SSCT*
                                 ↑ each points to one or more SSDTs
```

| Table   | What it tells Windows                   | Source               |
|---------|------------------------------------------|----------------------|
| RSDP    | Root signature + table address           | EDK II Auto-gen      |
| MADT    | CPU topology + GIC interrupt routing     | ssdt_cpu.asl + manual |
| FACP    | Reset register, shutdown behavior        | Manual (ACPI spec)   |
| MCFG    | PCIe MMIO config space mapping           | Manual (PCIe BARs)   |
| HPET    | System timer for OS tick                 | Auto-gen by acpidump |
| SSDT_CPU| Each Processor object (_HID/_UID/_STA)   | ssdt_cpu.asl         |
| SSDT_PERIPH| UART, GPIO, I2C, WDT devices          | ssdt_periph.asl      |

Key challenge: MADT's **GICC entries** must match the actual GIC distributor
base (`0x00B1000000` per SDM660 TRM) and IRQ routing. If the offset is wrong,
Windows will misroute interrupts and hang during early SMP bringup.

See [acpi-notes.md](acpi-notes.md) for full register-by-register MADT layout.

---

## 4. Driver Dependency Graph

```
Platform.c
  ├── ClocksGpcd      ← needs no predecessor; called first
  ├── PmicRegulator   ← also early; supplies voltages
  ├── StorageUfs      ← after clocks run; needed for boot media
  ├── UsbHost         ← optional; available once USB PHY clocked
  ├── TouchI2c        ← depends on ClocksGpcd (I2C clock source)
  └── WifiQca         ← last; needs PCIe enumeration (deferred to Phase 2)
```

---

## 5. Known Open Problems

| # | Problem                                              | Impact                    | Priority |
|---|-------------------------------------------------------|---------------------------|----------|
| 1 | GPLL lock timeout value unverified on real silicon    | Clock driver may fail     | High     |
| 2 | UFS LBA geometry not yet read from real device        | Storage driver reports fake capacity | High |
| 3 | PCIe BAR addresses for WiFi unknown without HW debug  | WifiQca remains stub      | Medium   |
| 4 | No working SPMI I2C bus driver on PMI632              | PMIC rail reads/writes are writes-only (no feedback) | Medium |
| 5 | MADT GICC interrupt number mismatch possible          | SMP boot fails silently   | High     |

---

## 6. References

- SDM660 TRM (Qualcomm internal — not public; use kernel DTSI as proxy)
- EDK II docs: https://github.com/tianocore/edk2/tree/master/MdeModulePkg/Doc
- postmarketOS jason page: https://devices.postmarketos.org/device/jason/
- Linux kernel `arch/arm64/boot/dts/qcom/sdm660.dtsi` (online at android.googlesource.com)
