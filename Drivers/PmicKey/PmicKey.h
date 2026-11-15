/*
 * jason PMIC key protocol.
 *
 * Power key and volume down arrive through the PM660 PON block (pon_1 /
 * pon_2 in the DTB), volume up is a plain PM660L GPIO. All three are SPMI
 * peripherals, so this driver only works when Drivers/SpmI is present.
 */
#pragma once

#include <Uefi.h>

#define JASON_PMIC_KEY_PROTOCOL_SIGNATURE  SIGNATURE_32 ('P', 'M', 'K', 'Y')

typedef struct {
  BOOLEAN    PowerPressed;
  BOOLEAN    VolumeUpPressed;
  BOOLEAN    VolumeDownPressed;
} JASON_PMIC_KEY_STATE;

typedef struct _JASON_PMIC_KEY_PROTOCOL JASON_PMIC_KEY_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *JASON_PMIC_KEY_GET_STATE) (
  IN  JASON_PMIC_KEY_PROTOCOL  *This,
  OUT JASON_PMIC_KEY_STATE     *State
  );

struct _JASON_PMIC_KEY_PROTOCOL {
  UINT32                    Signature;
  JASON_PMIC_KEY_GET_STATE  GetState;
};

extern EFI_GUID  gJasonPmicKeyProtocolGuid;
