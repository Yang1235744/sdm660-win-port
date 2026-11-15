[Defines]
  PLATFORM_NAME                  = JASON
  PLATFORM_GUID                  = EC87D9B2-A1F3-4D78-B2CF-12DE9FAE67CB
  PLATFORM_VERSION               = 0.1.0
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/JasonUefi
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = UefiPlatform/Jason.fdf

[Packages]
  MdePkg/MdePkg.dec
  MdeModulePkg/MdeModulePkg.dec
  ArmPkg/ArmPkg.dec
  FatPkg/FatPkg.dec
  UefiPlatform/UefiPlatform.dec

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  TimerLib|ArmPkg/Library/ArmGenericTimerPhyCounterLib/ArmGenericTimerPhyCounterLib.inf
  CacheMaintenanceLib|ArmPkg/Library/ArmCacheMaintenanceLib/ArmCacheMaintenanceLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf

[Components]
  UefiPlatform/Platform.inf
  AcpiTables/AcpiTables.inf
  Drivers/ClocksGpcd/ClocksGpcd.inf
  Drivers/StorageUfs/StorageUfs.inf
  Drivers/UsbHost/UsbHost.inf
  Drivers/TouchI2c/TouchI2c.inf
  Drivers/PmicRegulator/PmicRegulator.inf
  Drivers/WifiQca/WifiQca.inf

  # PMIC bus + system status: SPMI arbiter is the base for keys and battery.
  Drivers/SpmI/SpmI.inf
  Drivers/PmicKey/PmicKey.inf
  Drivers/PmicBattery/PmicBattery.inf

  # USB host stack: UsbHost publishes the DWC3 xHCI MMIO window as a
  # non-discoverable PCI device; PciBus/XhciDxe then enumerate it.
  MdeModulePkg/Bus/Pci/NonDiscoverablePciDeviceDxe/NonDiscoverablePciDeviceDxe.inf
  MdeModulePkg/Bus/Pci/PciBusDxe/PciBusDxe.inf
  MdeModulePkg/Bus/Pci/XhciDxe/XhciDxe.inf
  MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf

  # Make the UFS / USB block devices visible as file systems.
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf

[PcdsFeatureFlag]
  gJasonPlatformTokenSpaceGuid.PcdTouchEnabled|TRUE
  gJasonPlatformTokenSpaceGuid.PcdUfsEnabled|TRUE
  gJasonPlatformTokenSpaceGuid.PcdPmicEnabled|TRUE

[PcdsFixedAtBuild]
  gJasonPlatformTokenSpaceGuid.PcdBoardRevId|0x00020001
  gJasonPlatformTokenSpaceGuid.PcdNumCores|8
  gJasonPlatformTokenSpaceGuid.PcdPanelWidth|1080
  gJasonPlatformTokenSpaceGuid.PcdPanelHeight|2160
  gJasonPlatformTokenSpaceGuid.PcdUfsPresent|TRUE
  gJasonPlatformTokenSpaceGuid.PcdDefaultBootMode|0
  gJasonPlatformTokenSpaceGuid.PcdWifiChipset|0
  gJasonPlatformTokenSpaceGuid.PcdUartBaudRate|115200

  # jason DTB /soc/interrupt-controller@17a00000: GICD, GICR, GICR length.
  gJasonPlatformTokenSpaceGuid.PcdGicDistributorBase|0x0000000017A00000
  gJasonPlatformTokenSpaceGuid.PcdGicRedistributorBase|0x0000000017B00000
  gJasonPlatformTokenSpaceGuid.PcdGicRedistributorLength|0x0000000000100000
