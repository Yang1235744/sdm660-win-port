# Roadmap — Milestones & Target Dates

## Phase 0: Project Setup ✅ (2026-Q3)
- [x] Create repository structure under `J:\文档\Trae-CN\temp_20261114_02`
- [x] Write `README.md`, `LICENSE`, `.gitignore`
- [x] Generate first UEFI platform skeleton (DSC, INF, Platform.h)
- [x] Draft ACPI CPU/Peripheral tables (ASL)
- [ ] Write `docs/arch.md` ✅ done above
- [ ] Push to GitHub repo `sdm660-win-port` under user account

## Phase 1: Clock + PMIC Verification ⏳ (Target: 2026-Q4)
Goal: get the board to print to UART and stabilize power rails.
- [ ] Obtain a Qualcomm SDM660 TRM section covering GCC (Global Clock Controller)
- [ ] Run `dmesg \| grep -i gpll` on a jason running Android to extract real GPLL0 rate
- [ ] Flash a minimal UEFI PEI stage that only enables console UART
- [ ] Confirm `InitConsoleUart()` actually prints to a serial terminal
- [ ] Wire `BringUpClockTree()` and verify GPLL_LOCK_STATUS_BIT after ~1 ms
- [ ] Integrate `PmicInitAllRails()` — confirm VDDCX is stable before CPU0 starts

## Phase 2: Storage & Boot Media ⏳ (Target: 2026-Q4 – Q1)
Goal: enumerate UFS/eMMC and mount a FAT partition containing `bootmgfw.efi`.
- [ ] Reverse-engineer the UFS HC register map (SDM660 TRM §26)
- [ ] Validate `UfsReadSectors()` against a known-good sector on the device
- [ ] Implement a minimal FAT12/16/32 filesystem reader in C (no full EDK FatLib)
- [ ] Load an arm64 `bootmgfw.efi` from the FAT root
- [ ] Confirm the Windows bootloader image loads into memory correctly

## Phase 3: ACPI Conformance ✅ Partial (Ongoing)
Goal: Windows ARM64 successfully parses the RSDT/MADT.
- [ ] Fill in the missing MADT entries (GICC, GSI to IRQ mapping)
- [ ] Compile `gcc_tables.py` → verify `.aml` sizes < 4 KB each
- [ ] Inject compiled blobs into the UEFI firmware volume (`FvMain`)
- [ ] Boot a Windows ARM64 preview build and check dmesg for ACPI parse errors

## Phase 4: Display Output (Target: 2027-Q1)
Goal: render anything on the 1080×2160 OLED panel.
- [ ] Implement MDSS display engine driver (DMA → LCD controller)
- [ ] Write a framebuffer protocol implementation (simple VGA-compatible output)
- [ ] Enable display backlight via PMIC LDO control
- [ ] Show Windows logo via early boot DXE driver

## Phase 5: Input & Peripherals (Target: TBD)
- [ ] Touch input via `TouchI2c` → HID touch panel class
- [ ] USB host stack for keyboard/mouse (Phase 2 prerequisite met)
- [ ] Wi-Fi driver for internet tethering during install
- [ ] Power management (CPU idle states, suspend-to-RAM)

## Phase 6: Full Windows on Snapdragon 660 🎯 (Long-term)
- Full Win11 ARM64 installation via UEFI shell or Fastboot
- Working graphics acceleration (Adreno 512 DirectX 12 support — ambitious)
- Audio, camera, cellular data

---

## Contributing Checklist

When submitting a PR, please ensure:
- [ ] All C code compiles cleanly with `-Wall -Wextra` (clang recommended)
- [ ] New ACPI registers cross-checked against at least one dmesg dump
- [ ] Hardware-tested binary attached as a `.bin` artifact (do NOT commit to git)
- [ ] Update `roadmap.md` milestones if you complete one
