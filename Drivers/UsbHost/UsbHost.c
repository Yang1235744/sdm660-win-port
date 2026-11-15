/*
 * jason USB host: expose the DWC3 xHCI controller to the standard EDK2 stack.
 *
 * DTB /soc/ssusb@a800000 contains dwc3@a800000 (snps,dwc3) at 0x0A800000.
 * The DWC3 xHCI register block occupies the first 0xC100 bytes of that window;
 * the device registers start at 0xC100. There is no PCIe root port on jason,
 * so the controller is published as an MMIO non-discoverable PCI device and
 * left to PciBusDxe + XhciDxe + UsbBusDxe + UsbMassStorageDxe to enumerate.
 *
 * This driver only registers resources. It does not touch the USB PHY, the
 * GCC USB clocks or the second (DTB-disabled) controller at 0x0C200000.
 */

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/NonDiscoverableDevice.h>

#include <Hardware/Platform.h>

STATIC
EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  mXhciResources[] = {
  {
    ACPI_ADDRESS_SPACE_DESCRIPTOR,
    (UINT16)(sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3),
    ACPI_ADDRESS_SPACE_TYPE_MEM,
    0,                              /* GenFlag                        */
    0,                              /* SpecificFlag                   */
    0,                              /* AddrSpaceGranularity           */
    DWC3_BASE,                      /* AddrRangeMin                   */
    DWC3_BASE + DWC3_XHCI_SIZE - 1, /* AddrRangeMax                   */
    0,                              /* AddrTranslationOffset          */
    DWC3_XHCI_SIZE                  /* AddrLen                        */
  },
  {
    ACPI_END_TAG_DESCRIPTOR,
    0
  }
};

STATIC CONST NON_DISCOVERABLE_DEVICE  mXhciDevice = {
  &gEdkiiNonDiscoverableXhciDeviceGuid,
  NonDiscoverableDeviceDmaTypeNonCoherent,
  NULL,
  mXhciResources
};

EFI_STATUS
EFIAPI
UsbHostDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID *)&mXhciDevice,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Usb: failed to register xHCI at 0x%lx: %r\n", (UINT64)DWC3_BASE, Status));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "Usb: xHCI at 0x%lx size 0x%x registered for PciBus/XhciDxe\n",
          (UINT64)DWC3_BASE, (UINT32)DWC3_XHCI_SIZE));
  return EFI_SUCCESS;
}
