# Xiaomi Note3 (jason) — SDMLinux EFI Project for Windows ARM64 Boot

> A community effort to port Windows ARM64 onto Qualcomm Snapdragon 660 devices
> (Xiaomi Mi Note 3 / jason). This project provides a custom UEFI firmware image
> and ACPI tables that allow `bootmgfw.efi` to boot on top of the Android/Linux
> boot chain.

## ⚠️ Disclaimer

This is **experimental research-grade firmware**. It has NOT been tested on any
real hardware yet. Building this requires reverse-engineered register maps from
Qualcomm's SDM660 BSP. Proceed at your own risk — flashing invalid firmware can
brick the device permanently.

The project is released under **Apache-2.0** license. See [LICENSE](LICENSE).

---

## What We Build

| Component | Purpose |
|-----------|---------|
| **UEFI DXE Core & Payload** | Drops into Android's boot flow via DTB overlay or kernel module, loads `bootmgfw.efi` from FAT partition |
| **ACPI Tables (RSDP/SSDT)** | Describe SDM660's SoC peripherals to Windows ARM64 so it can find display, storage, USB, touch, WiFi, PMIC |
| **ARM64 Driver Skeletons** | Reference implementations for GPCD/GPIO clocks, UFS/eMMC storage, USB 2.0 host controller, I2C touchpad, QCA Wi-Fi, and Power Management |

## Hardware Target

- **Device**: Xiaomi Mi Note 3 (codename: `jason`)
- **SoC**: Qualcomm Snapdragon 660 (SDM660) — Kryo 260 (A73×4 + A53×4), Adreno 512
- **Boot CPU**: Primary boot always runs on **CPU0 (A53)**, UEFI firmware must be **AArch64** (`arm64` ABI)
- **Memory Map**: ~4 GB LPDDR4; UEFI region reserved at start of RAM (see `Platform.h` for carve-out addresses)

## Prerequisites (Development Host — Windows)

```bash
# 1. Python 3.9+
# 2. edk2-stable202411 or later (from https://github.com/tianocore/edk2)
git clone https://github.com/tianocore/edk2.git
cd edk2 && git submodule update --init --recursive

# 3. ARM64 toolchain (EDK2's default arm-toolchain works for build; real
#    signing/validation still needs Qualcomms' proprietary tools)
pip install pipwin
pipwin install armcc  # optional, for some vendor modules

# 4. acpica source (for iASL table compiler)
wget https://sourceforge.net/projects/acpica/files/iasl/20240322/iasl-linux64.zip/download -O iasl.zip
unzip iasl.zip && chmod +x iasl
```

## Build

```bash
# Clone this repo alongside EDK2
git clone https://github.com/<your-user>/sdm660-win-port.git
cd sdm660-win-port

# Source EDK2 environment
export WORKSPACE=$(pwd)/../edk2
export EDK_TOOLS_PATH=$WORKSPACE/BaseTools
export INF_EXTRA_INCLUDE_PATH=$WORKSPACE/MdeModulePkg
export PACKAGES_PATH=$(pwd):$WORKSPACE

# Build the firmware
python edksetup.py build --platform JASON:XIAOMI_NOTE3:NULL
  -a AARCH64
  -b RELEASE
  -t GCC5
  -p UefiPlatform/UefiPlatform.dsc
```

Build artifacts appear in `Build/JasonUefi/DEBUG_GCC5/FV/`. The combined FV blob
is written into the `bootmgfw.efi` entry point inside the vendor recovery image.

## Repository Layout

```
├── UefiPlatform/            ← Main UEFI/EDK2 platform
│   ├── UefiPlatform.dsc     ← EDK II Device Specific Configuration
│   ├── Platform.c           ← Boot entry, DTB injection, payload launcher
│   └── Include/Hardware/    ← Register maps (reverse-engineered offsets)
│
├── AcpiTables/              ← RSDP + SSDT generation
│   ├── rsdp.asl             ← Root System Description Pointer
│   ├── ssdt_cpu.asl         ← CPU hot-plug descriptors (4 big + 4 little)
│   ├── ssdt_periph.asl      ← UART, SPI, GPIO controller mapping
│   └── gcc_tables.sh        ← Bash helper to compile ASL → ACPI binary blobs
│
├── Drivers/                 ← ARM64 driver reference skeletons
│   ├── ClocksGpcd.c         ← SDM660 GPLL/VCO clock tree driver
│   ├── StorageUfs.c         ← UFS HC (SDM660 eMMC/ufs bridge)
│   ├── UsbHost.c            ← DWC2 USB 2.0 OTG controller driver
│   ├── TouchI2c.c           ← I2C SLIMBUS touch panel driver
│   ├── WifiQca.c            ← QCA6174/Ath10k PCIe WiFi driver stub
│   └── PmicRegulator.c      ← PMI632 PMIC LDO/Buck control driver
│
├── Scripts/                 ← Host-side tooling
│   └── InjectPayload.py     ← Android → UEFI chain switcher
│
└── Docs/                    ← Design documentation
    ├── arch.md              ← Architecture overview
    ├── hardware-notes.md    ← Known register layout (incomplete!)
    └── roadmap.md           ← Milestone tracking
```

## Roadmap / Current Status

| Phase | Milestone | Target Date |
|-------|-----------|-------------|
| Phase 1 | UEFI boot payload injects into Android initramfs, prints to UART | 2026-Q4 |
| Phase 2 | ACPI tables recognized by ARM64 Linux as intermediate step | 2026-Q1 |
| Phase 3 | Windows ARM64 bootmgfw.efi starts (display + basic cursor) | TBD |
| Phase 4 | Full Windows stack with touch input, keyboard, display rotation | TBD |

### Hardware Knowledge Gaps (need reverse-engineering)

1. **[OPEN]** SDM660 PLATCTL base address — exact offset to read/write bootloader unlock bits
2. **[OPEN]** Kernel DTB node names for `qcom,gpnvm`, `qcom,tcs-clk` — used by ClocksGpcd.c
3. **[OPEN]** UFS device tree binding string: `qcom,ufs-hc-v2` is assumed but unverified
4. **[OPEN]** QCA6174 PCIe function number on the SoC bus — needed for `WifiQca.c`
5. **[KNOWN]** ADC_TM peripheral: `0x01800000` (base) — used by PMIC regulator sampling

## Contributing

See [CONTRIBUTING.md](Docs/contributing.md) for development workflow notes.
Pull requests welcome — please include a hardware validation log if you test on
real jason silicon.

## Related Projects

- [linux-on-qcom-devices](https://github.com/semihalf/linux-on-qcom-devices) — postmarketOS jason support
- [Windows-on-ARM-Patch](https://github.com/nickvoid/windows-arm-patch) — general Win11 ARM64 patching
- [Redroid](https://github.com/remote-android/redroid) — Android container runtime (UI rendering)
- [Phoenix OS](https://github.com/peter-tse/phoenixos) — x86 Android emulation (reference for UI design)
