/*
 * ClocksGpcd.c — GCC clock placeholder for jason.
 *
 * The previous version of this file programmed invented GCC offsets derived
 * from a fixed 0x400 stride. That does not match the real SDM660 GCC layout,
 * where every branch has its own offset (for example the UFS branches live at
 * 0x75008/0x7500C and 0x76008, not at a linear stride). Nothing here may write
 * a clock register until each offset is taken from a verified source.
 *
 * Verified UFS clock data is implemented inside Drivers/StorageUfs.
 */

#include <Uefi.h>

EFI_STATUS
EFIAPI
GpcdDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_UNSUPPORTED;
}
