/*
 * WifiQca.c — unsupported Wi-Fi placeholder for jason.
 *
 * Source: matching jason boot.img DTB /soc/qcom,icnss@18800000 and
 * /bt_wcn3990. The device uses WCN3990/ICNSS; this DTB contains no PCIe host
 * controller or evidence for the legacy QCA6174/ath10k PCIe assumptions.
 * Firmware loading, SMMU, remote-processor and power sequencing are not
 * implemented, so no MMIO, PCI or firmware operation is permitted here.
 */

#include <Uefi.h>

EFI_STATUS
EFIAPI
WifiQcaDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
WifiInit (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}
