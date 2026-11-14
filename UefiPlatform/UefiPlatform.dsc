/**
 * SDM660 UEFI Firmware Description File (DSC)
 *
 * EDK II Device Specific Configuration — tells PCDs which modules belong
 * in this target platform and links them into the firmware volume.
 *
 * Platform : JASON:XIAOMI_NOTE3:NULL   (per BuildConfig.bat convention)
 * Target   : AARCH64 RELEASE
 */
[Defines]
  INF_FILE_FORMAT                          = FALSE
  ARCHITECTURE                             = AARCH64
  TOOL_CHAIN_TAG                           = GCC5
  PLATFORM_NAME                            = JASON
  PLATFORM_VERSION                         = 0.1.0
  PLATFORM_DESCRIPTION                     = Qualcomm SDM660 Windows ARM64 Port — UEFI firmware payload
  PLATFORM_GUID                            = EC87D9B2-A1F3-4d78-B2CF-12DE9FAE67CB
  PLATFORM_CORE_ID                         = 0
  PLATFORM_MAX_NUM_CPUS                    = 8
  PLATFORM_IMAGE_LIBRARY_PATH              = {}
  OUTPUT_DIR                             = Build/JasonUefi

# ── Packages ──────────────────────────────────────────────────────────────
[Packages]
  MdePkg/MdePkg.dec
  MdeModulePkg/MdeModulePkg.dec
  ArmPkg/ArmPkg.dec
  ArmVirtPkg/ArmVirtPkg.dec
  JASON/UefiPlatform.dec

# ── Library Instances ─────────────────────────────────────────────────────
[LibraryClasses]
  BaseLib|ArmPkg/Library/BaseLib/Arm/base.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
 IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  CacheMaintenanceLib|ArmPkg/Library/CacheMaintenanceLib/ArmCacheMaintenanceLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/RuntimeAllocatorLib/RuntimeAllocatorLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  SerialPortLib|MdeModulePkg/Library/DxeSerialPortLib/DxeSerialPortLib.inf

# ── Components ─────────────────────────────────────────────────────────────
[Components]
  # Core DXE driver — main entry point, DTB injection, boot path selection
  UefiPlatform/Platform.inf

  # ACPI table generation driver (called at PEI → DXE transition)
  UefiPlatform/AcpiTables.inf

  # Hardware abstraction layer drivers
  Drivers/ClocksGpcd/ClocksGpcd.inf
  Drivers/StorageUfs/StorageUfs.inf
  Drivers/UsbHost/UsbHost.inf
  Drivers/TouchI2c/TouchI2c.inf
  Drivers/PmicRegulator/PmicRegulator.inf

  # Optional: full Windows-compatible driver surface (uncomment when verified)
  # Drivers/WifiQca/WifiQca.inf

# ── PCDs — Platform Configuration Data ────────────────────────────────────
[pcd]
  # Board revision identifier (used by ACPI tables as _SUB suffix)
  gJasonPlatformTokenSpaceGuid.PcdBoardRevId           | 0x00020000 | UINT32 | 0x10000001
  # Number of cores visible to the bootloader (4 A53 + 4 A73)
  gJasonPlatformTokenSpaceGuid.PcdNumCores             | 8        | UINT8  | 0x10000002
  # Primary display panel DPI multiplier (0 = default 432p×960p)
  gJasonPlatformTokenSpaceGuid.PcdPanelDpiX            | 0x01B0   | UINT16 | 0x10000003
  # UFS media size in sectors (must match actual flash geometry)
  gJasonPlatformTokenSpaceGuid.PcdUfsCapacitySectors   | 0x00800000 | UINT32 | 0x10000004
  # Whether to inject a custom DTB overlay before handing off to the EFI payload
  gJasonPlatformTokenSpaceGuid.PcdInjectDtbOverlay     | TRUE     | BOOLEAN | 0x10000005
  # Boot mode selector — 0=UEFI default, 1=Force recovery, 2=Fastboot passthrough
  gJasonPlatformTokenSpaceGuid.PcdBootModeOverride     | 0x0    | UINT8  | 0x10000006
  # WiFi chipset enum (0=QCA6174, 1=Ath10k compatible clone)
  gJasonPlatformTokenSpaceGuid.PcdWifiChipset          | 0x0    | UINT8  | 0x10000007

# ── FV Image Layout ───────────────────────────────────────────────────────
[Fv]
  # Main firmware volume — holds ALL DXE drivers and the final boot manager
  FV = JASON_MAIN_FV,
       UniqueId = JASON_PLATFORM_GUID
  # Embedded FAT partition with bootmgfw.efi loader stub
  FILE FV_IMAGE = JASON_BOOT_FV,
       EntryPoint = JASON_BOOT_ENTRY,
       Section[0] = DxeCore
  # ACPI blob that Windows reads at runtime (pre-populated at build time)
  FILE ACPI_DATA = JASON_ACPI_DATA,
       Section[0] = AcpiTablesBlob

# ── Build Options ───────────────────────────────────────────────────────────
[BUILD_OPTIONS]
  STRIP = -s
  OPTIMIZE_FLAG = -O2
