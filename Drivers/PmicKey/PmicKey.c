/*
 * jason PMIC key driver.
 *
 * Sources (all from the matching boot.img DTB and the downstream PMIC code):
 *
 *   power key        PM660 PON pon_1, linux,code = 0x74 (KEY_POWER)
 *   volume down      PM660 PON pon_2, linux,code = 0x72 (KEY_VOLUMEDOWN)
 *   volume up        PM660L GPIO 7, gpios = <&pm660l_gpios 7 1>, active low
 *
 * The PON real time status reports the debounced line, so a set bit means the
 * key is held down. The GPIO real time status reports the pin level, and the
 * DTB marks the volume up line active low, so there a clear bit means pressed.
 *
 * This driver only observes the keys at UEFI time. Routing them to Windows
 * needs an OS side driver, which is described in Docs/hardware-notes.md.
 */

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Hardware/Platform.h>
#include "../SpmI/SpmI.h"
#include "PmicKey.h"

STATIC
VOID
PmicKeyLog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  CHAR8    Buffer[128];
  VA_LIST  Marker;

  VA_START (Marker, Format);
  AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);
  DEBUG ((DEBUG_INFO, "PmicKey: %a\n", Buffer));
}

STATIC
EFI_STATUS
EFIAPI
PmicKeyGetState (
  IN  JASON_PMIC_KEY_PROTOCOL  *This,
  OUT JASON_PMIC_KEY_STATE     *State
  )
{
  EFI_STATUS             Status;
  JASON_SPMI_PROTOCOL    *SpmI;
  UINT8                  Pon;
  UINT8                  Gpio;

  if ((This == NULL) || (State == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (&gJasonSpmIProtocolGuid, NULL, (VOID **)&SpmI);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ZeroMem (State, sizeof (*State));

  Pon = 0;
  Status = SpmI->Read (SpmI, PM660_SID, PMIC_PON_RT_STS, &Pon, 1);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  State->PowerPressed      = ((Pon & PMIC_PON_KPDPWR_BIT) != 0);
  State->VolumeDownPressed = ((Pon & PMIC_PON_RESIN_BIT) != 0);

  Gpio   = 0;
  Status = SpmI->Read (
                   SpmI,
                   PM660L_SID,
                   PM660L_GPIO_BASE + (PM660L_GPIO_VOL_UP_PIN - 1U) * PM660L_GPIO_STRIDE + PM660L_GPIO_RT_STS,
                   &Gpio,
                   1
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Active low: the pin reads 0 while the key is pressed. */
  State->VolumeUpPressed = ((Gpio & BIT0) == 0);

  return EFI_SUCCESS;
}

STATIC JASON_PMIC_KEY_PROTOCOL  mPmicKeyProtocol = {
  JASON_PMIC_KEY_PROTOCOL_SIGNATURE,
  PmicKeyGetState
};

EFI_STATUS
EFIAPI
PmicKeyDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS           Status;
  JASON_PMIC_KEY_STATE State;
  VOID                 *SpmI;

  if (!FeaturePcdGet (PcdPmicEnabled)) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gJasonSpmIProtocolGuid, NULL, &SpmI);
  if (EFI_ERROR (Status)) {
    PmicKeyLog ("SPMI master missing, keys unavailable");
    return EFI_SUCCESS;
  }

  Status = PmicKeyGetState (&mPmicKeyProtocol, &State);
  if (EFI_ERROR (Status)) {
    PmicKeyLog ("key read failed (%r)", Status);
    return EFI_SUCCESS;
  }

  PmicKeyLog (
    "state power=%d vol_up=%d vol_down=%d",
    (UINTN)(State.PowerPressed != 0),
    (UINTN)(State.VolumeUpPressed != 0),
    (UINTN)(State.VolumeDownPressed != 0)
    );

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gJasonPmicKeyProtocolGuid,
                  &mPmicKeyProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    PmicKeyLog ("protocol install failed (%r)", Status);
  }

  return EFI_SUCCESS;
}
