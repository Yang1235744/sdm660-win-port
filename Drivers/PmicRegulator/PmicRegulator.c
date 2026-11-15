/*
 * PmicRegulator.c — PMIC placeholder for jason.
 *
 * DTB evidence: the SPMI arbiter at 0x0800F000 exposes qcom,pm660@0/1 and
 * qcom,pm660l@2/3. There is no PMI632 on this device, and the previous version
 * of this file used a flat MMIO SPMI model with invented LDO/BUCK offsets.
 *
 * SPMI is a two-wire bus with an arbiter; regulator access needs the SPMI
 * transport plus PM660/PM660L register semantics. Neither is implemented, so
 * this driver must not touch any rail.
 */

#include <Uefi.h>

EFI_STATUS
EFIAPI
PmicRegulatorDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_UNSUPPORTED;
}
