# SDM660 WinPort — Quick reference for developers
# ====================================================

## Repository layout

```
UefiPlatform/           EDK II platform descriptor + main DXE module
  ├── UefiPlatform.dsc  ← edit this to add/remove drivers
  ├── Platform.c        ← entry point; calls each driver's Init*() function
  └── Include/Hardware/
      └── Platform.h    ← register offsets — update these when TRM is consulted

AcpiTables/             ASL source for ACPI tables
  ├── ssdt_cpu.asl      ← CPU topology (8 cores: 4 big + 4 little)
  ├── ssdt_periph.asl   ← UART, I2C, GPIO, WDT device descriptors
  ├── rsdp.asl          ← RSDP stub — actual RSDP created at PEI via protocol
  └── gcc_tables.py     ← compile .asl → .aml using iASL

Drivers/                ARM64 driver skeletons
  ├── ClocksGpcd/       GPLL/VCO clock tree driver
  ├── StorageUfs/       UFS host controller driver
  ├── UsbHost/          DWC2 USB 2.0 OTG driver
  ├── TouchI2c/         QUP I2C1 → touch panel
  ├── PmicRegulator/    PMI632 PMIC LDO/Buck voltage & enable driver
  └── WifiQca/          Unsupported placeholder; jason uses WCN3990/ICNSS, not QCA6174 PCIe

Docs/                   Design notes (write up your findings here)
  ├── arch.md           ← overall boot flow diagram
  ├── hardware-notes.md ← evolving register map as you verify
  └── roadmap.md        ← milestone checklist

Scripts/
  └── InjectPayload.py  ← Android boot chain hook (inject UEFI payload via DTB overlay)
```

## Conventions used throughout

| Item                        | Rule                                                                      |
|-----------------------------|---------------------------------------------------------------------------|
| Register names              | `REGNAME_OFFSET` in C — e.g. `GCC_GPLL0_MODE`                             |
| Hex literals                | `0x…` suffix — no bare octal                                              |
| Comments                    | Chinese or English accepted; prefer concise technical prose               |
| Driver struct init pattern  | `(EFI_STATUS EFIAPI NameInit)(VOID)`                                      |
| ACPI IRQ values             | Match GSI numbers from sdm660.dtsi — never invent without hardware proof  |

## How a new driver gets added

1. Create `Drivers/<Name>/<Name>.c` (and optionally `<Name>.h`)
2. Write a corresponding `<Name>.inf` section (see existing `.inf` entries)
3. Add the driver line to `UefiPlatform.dsc` → `[Components]` block
4. Call its `NameInit()` from `Platform.c::InitializePlatform()`
5. Update `ROADMAP.md` with which hardware it targets

## Common pitfalls on jason-specific hardware

- **GPLL lock timeout**: The VCO may need ~5 ms to stabilize after reset.
  If `BringUpClockTree()` returns DEVICE_ERROR, try increasing the retry count
  in Platform.c — some silicon revisions are slower.
- **Pinmux conflicts**: TLMM pin functions overlap between display, WiFi, and
  BT. Only one peripheral can claim a pin at a time; check pinctrl nodes in
  sdm660-jason.dts before assigning.
- **PMIC rail sequencing**: VDDCX must be stable BEFORE GPLL enables; reverse
  order causes CPU instability on first boot. The current code sets core then
  memory — adjust if your board revision differs.
- **ACPI IRQ assignment**: The GSI numbers written into `_CRS` MUST match what
  the kernel reports in `/proc/interrupts`. Use `dmesg | grep -i gsi` on the
  running Android system to verify each value.

## Next concrete steps (in rough priority order)

1. [ ] Verify each register base address against `dmesg | grep memblock` on
       a flashed jason unit (need a working TTY bootloader).
2. [ ] Port `UsbHost.c` to an actual DWC2 enumeration test on hardware.
3. [ ] Build the AcpiTables blob and inject it into the RSDT of the EDK II FV.
4. [ ] Get a Windows ARM64 WIM image that boots on Snapdragon platforms
       (PhoenixOS / Win11 ARM reference builds).
5. [ ] Write a minimal FAT driver so `bootmgfw.efi` can be read from storage.
6. [ ] Document every register offset you confirm on real silicon in
       Docs/hardware-notes.md — this is the most valuable long-term output
       of the whole project.
