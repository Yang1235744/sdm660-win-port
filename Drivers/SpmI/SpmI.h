/*
 * jason SPMI master protocol.
 *
 * The PMIC (PM660 / PM660L) is not memory mapped: battery, power key, volume
 * keys and every regulator live behind the SPMI two-wire bus. This protocol is
 * the only firmware entry point to that bus, so the address decoding stays in
 * one place instead of being duplicated per driver.
 */
#pragma once

#include <Uefi.h>

#define JASON_SPMI_PROTOCOL_SIGNATURE  SIGNATURE_32 ('S', 'P', 'M', 'I')

typedef struct _JASON_SPMI_PROTOCOL JASON_SPMI_PROTOCOL;

/**
  Read 1..8 bytes from a PMIC peripheral register.

  @param[in]  This    Protocol instance.
  @param[in]  Sid     SPMI slave id (PM660 = 0, PM660L = 2).
  @param[in]  Addr    Peripheral register address, for example 0x0810.
  @param[out] Buffer  Destination, at least Length bytes.
  @param[in]  Length  Byte count, 1..8.
**/
typedef
EFI_STATUS
(EFIAPI *JASON_SPMI_READ) (
  IN  JASON_SPMI_PROTOCOL  *This,
  IN  UINT8                Sid,
  IN  UINT16               Addr,
  OUT VOID                 *Buffer,
  IN  UINTN                Length
  );

/**
  Write 1..8 bytes to a PMIC peripheral register.
**/
typedef
EFI_STATUS
(EFIAPI *JASON_SPMI_WRITE) (
  IN JASON_SPMI_PROTOCOL  *This,
  IN UINT8                Sid,
  IN UINT16               Addr,
  IN CONST VOID           *Buffer,
  IN UINTN                Length
  );

struct _JASON_SPMI_PROTOCOL {
  UINT32             Signature;
  JASON_SPMI_READ    Read;
  JASON_SPMI_WRITE   Write;
};

extern EFI_GUID  gJasonSpmIProtocolGuid;
