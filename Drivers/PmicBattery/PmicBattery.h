/*
 * jason battery protocol.
 *
 * Reports state of charge out of the PM660 fuel gauge (gen3). The same numbers
 * are what a Windows side battery driver would have to read, so the protocol
 * exists to keep that code path in one place.
 */
#pragma once

#include <Uefi.h>

#define JASON_BATTERY_PROTOCOL_SIGNATURE  SIGNATURE_32 ('B', 'A', 'T', 'T')

typedef struct {
  BOOLEAN    Present;
  UINT8      Percentage;   /* 0..100 */
} JASON_BATTERY_STATUS;

typedef struct _JASON_BATTERY_PROTOCOL JASON_BATTERY_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *JASON_BATTERY_GET_STATUS) (
  IN  JASON_BATTERY_PROTOCOL  *This,
  OUT JASON_BATTERY_STATUS    *Status
  );

struct _JASON_BATTERY_PROTOCOL {
  UINT32                      Signature;
  JASON_BATTERY_GET_STATUS    GetStatus;
};

extern EFI_GUID  gJasonBatteryProtocolGuid;
