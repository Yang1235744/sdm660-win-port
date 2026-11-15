/*
 * jason battery (PM660 fuel gauge, gen3) driver.
 *
 * DTB evidence: qpnp,fg with compatible "qcom,fg-gen3" on PMIC slave 0, with
 * the state-of-charge block at peripheral 0x4000.
 *
 * Register use follows drivers/power/supply/qcom/qpnp-fg.c (gen3):
 * get_monotonic_soc_raw() reads two bytes at soc_base + SOC_MONOTONIC_SOC
 * (0x09) and only accepts the sample when the two shadow bytes match; the
 * value is already a percentage and 0xFF means "full".
 *
 * Not implemented: current, voltage, temperature, charge counters and charger
 * state. Those live in the FG SRAM and in the SMB2 charger, whose register map
 * has not been verified for jason yet, so they are left out instead of guessed.
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
#include "PmicBattery.h"

#define FG_SOC_RETRIES  10U

EFI_GUID  gJasonBatteryProtocolGuid = {
  0xA7E3C058, 0x1D94, 0x4B62, { 0x8F, 0x21, 0x53, 0xBE, 0x6C, 0x08, 0xD7, 0x4A }
};

STATIC
VOID
PmicBatteryLog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  CHAR8    Buffer[128];
  VA_LIST  Marker;

  VA_START (Marker, Format);
  AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);
  DEBUG ((DEBUG_INFO, "Battery: %a\n", Buffer));
}

STATIC
EFI_STATUS
EFIAPI
PmicBatteryGetStatus (
  IN  JASON_BATTERY_PROTOCOL  *This,
  OUT JASON_BATTERY_STATUS    *Status
  )
{
  EFI_STATUS           Result;
  JASON_SPMI_PROTOCOL  *SpmI;
  UINT8                Sample[2];
  UINT32               Try;

  if ((This == NULL) || (Status == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Status, sizeof (*Status));

  Result = gBS->LocateProtocol (&gJasonSpmIProtocolGuid, NULL, (VOID **)&SpmI);
  if (EFI_ERROR (Result)) {
    return Result;
  }

  for (Try = 0; Try < FG_SOC_RETRIES; Try++) {
    Result = SpmI->Read (
                    SpmI,
                    PM660_SID,
                    (UINT16)(FG_SOC_BASE + FG_SOC_MONOTONIC_SOC),
                    Sample,
                    sizeof (Sample)
                    );
    if (EFI_ERROR (Result)) {
      return Result;
    }

    if (Sample[0] == Sample[1]) {
      Status->Present    = TRUE;
      Status->Percentage = (Sample[0] == FG_SOC_RAW_FULL) ? 100U : Sample[0];
      if (Status->Percentage > 100U) {
        Status->Percentage = 100U;
      }
      return EFI_SUCCESS;
    }

    MicroSecondDelay (1000);
  }

  return EFI_DEVICE_ERROR;
}

STATIC JASON_BATTERY_PROTOCOL  mBatteryProtocol = {
  JASON_BATTERY_PROTOCOL_SIGNATURE,
  PmicBatteryGetStatus
};

EFI_STATUS
EFIAPI
PmicBatteryDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS            Status;
  JASON_BATTERY_STATUS  Battery;
  VOID                  *SpmI;

  if (!FeaturePcdGet (PcdPmicEnabled)) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gJasonSpmIProtocolGuid, NULL, &SpmI);
  if (EFI_ERROR (Status)) {
    PmicBatteryLog ("SPMI master missing, gauge unavailable");
    return EFI_SUCCESS;
  }

  Status = PmicBatteryGetStatus (&mBatteryProtocol, &Battery);
  if (EFI_ERROR (Status)) {
    PmicBatteryLog ("gauge read failed (%r), no sample", Status);
  } else {
    PmicBatteryLog ("%d%% (PM660 FG gen3)", (UINTN)Battery.Percentage);
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gJasonBatteryProtocolGuid,
                  &mBatteryProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    PmicBatteryLog ("protocol install failed (%r)", Status);
  }

  return EFI_SUCCESS;
}
