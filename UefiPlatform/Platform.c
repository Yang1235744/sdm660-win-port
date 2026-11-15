#include <Uefi.h>
#include <Library/UefiLib.h>

/**
  jason platform driver entry point.

  The previous implementation wrote unverified UART, GCC and TLMM addresses as
  soon as DXE dispatched this module. Keep initialization disabled until the
  corresponding DTB nodes, clocks and power sequencing have been validated.
**/
EFI_STATUS
EFIAPI
InitializePlatform (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  Print (L"JASON: hardware initialization is disabled pending DTB validation\n");
  return EFI_UNSUPPORTED;
}
