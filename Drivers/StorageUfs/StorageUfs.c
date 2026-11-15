/*
 * jason UFS host controller driver.
 *
 * Scope: bring the UFS block up from a cold UEFI start, read the unit
 * descriptor and publish EFI_BLOCK_IO_PROTOCOL for LUN 0. Linux, Android and
 * the bootloader all drive the same controller, so the register level sequence
 * is kept close to UFS HCI 2.1 plus the SDM660 GCC clock gates.
 *
 * Deliberately not implemented (needs on-device data or a much larger port):
 *   - QMP UFS PHY serdes calibration tables. The bootloader normally leaves
 *     the PHY powered; the driver only re-enables PHY clocks and the UFS reset.
 *   - HS gear negotiation, so the link stays at PWM-G1 and is slow.
 *   - Inline crypto (ICE), RPMB, boot LUN switching, multiple LUNs.
 *
 * Every wait is bounded, so a controller that is not alive makes the driver
 * give up instead of hanging the boot.
 */

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#include <Hardware/Platform.h>
#include "UfsHci.h"

#define UFS_WAIT_STEP_US       10
#define UFS_HCS_TIMEOUT_US     100000U
#define UFS_UIC_TIMEOUT_US     500000U
#define UFS_CMD_TIMEOUT_MS     2000U
#define UFS_LINK_TIMEOUT_MS    1000U
#define UFS_BOUNCE_BYTES       (128U * 1024U)
#define UFS_MAX_PRD            (UFS_BOUNCE_BYTES / EFI_PAGE_SIZE)

typedef struct {
  UINTN                  HcBase;
  UFS_COMMAND_SLOT       *Slot;
  UINT8                  *Bounce;
  EFI_BLOCK_IO_MEDIA     Media;
  EFI_BLOCK_IO_PROTOCOL  BlockIo;
  EFI_HANDLE             Handle;
  UINT32                 BlockSize;
  UINT64                 LastLba;
} UFS_PRIVATE;

STATIC UFS_PRIVATE  mUfs;

STATIC
VOID
UfsLog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  CHAR8    Buffer[192];
  VA_LIST  Marker;

  VA_START (Marker, Format);
  AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);

  DEBUG ((DEBUG_INFO, "Ufs: %a\n", Buffer));
}

STATIC
EFI_STATUS
UfsPollHcs (
  IN UINTN   HcBase,
  IN UINT32  Mask,
  IN UINT32  Expected,
  IN UINTN   TimeoutUs
  )
{
  UINTN  Elapsed;

  for (Elapsed = 0; Elapsed < TimeoutUs; Elapsed += UFS_WAIT_STEP_US) {
    if ((MmioRead32 (HcBase + UFS_HC_HCS) & Mask) == Expected) {
      return EFI_SUCCESS;
    }
    MicroSecondDelay (UFS_WAIT_STEP_US);
  }
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
UfsSendUic (
  IN  UINTN   HcBase,
  IN  UINT32  Opcode,
  IN  UINT32  Arg1,
  IN  UINT32  Arg2,
  IN  UINT32  Arg3,
  OUT UINT32  *Result OPTIONAL
  )
{
  UINTN  Elapsed;

  if (UfsPollHcs (HcBase, UFS_HCS_UCRDY, UFS_HCS_UCRDY, UFS_HCS_TIMEOUT_US) != EFI_SUCCESS) {
    return EFI_TIMEOUT;
  }

  MmioWrite32 (HcBase + UFS_HC_IS, UFS_IS_UCCS);
  MmioWrite32 (HcBase + UFS_HC_UCMDARG1, Arg1);
  MmioWrite32 (HcBase + UFS_HC_UCMDARG2, Arg2);
  MmioWrite32 (HcBase + UFS_HC_UCMDARG3, Arg3);
  MmioWrite32 (HcBase + UFS_HC_UICCMD, Opcode);

  for (Elapsed = 0; Elapsed < UFS_UIC_TIMEOUT_US; Elapsed += UFS_WAIT_STEP_US) {
    if ((MmioRead32 (HcBase + UFS_HC_IS) & UFS_IS_UCCS) != 0) {
      if (Result != NULL) {
        *Result = MmioRead32 (HcBase + UFS_HC_UCMDARG2);
      }
      MmioWrite32 (HcBase + UFS_HC_IS, UFS_IS_UCCS);
      return EFI_SUCCESS;
    }
    MicroSecondDelay (UFS_WAIT_STEP_US);
  }

  return EFI_TIMEOUT;
}

/*
 * Enable a GCC branch clock and wait for it to leave the halt state.
 * CBCR layout is shared by the SDM660 branches used below:
 * bit 0 = CLK_ENABLE, bit 31 = CLK_OFF (1 means halted).
 */
STATIC
VOID
UfsEnableGccBranch (
  IN UINT32  Offset
  )
{
  UINT32  Value;
  UINTN   Spin;

  Value  = MmioRead32 (GCC_BASE + Offset);
  Value |= BIT0;
  MmioWrite32 (GCC_BASE + Offset, Value);

  for (Spin = 0; Spin < 1000; Spin++) {
    if ((MmioRead32 (GCC_BASE + Offset) & BIT31) == 0) {
      return;
    }
    MicroSecondDelay (10);
  }

  DEBUG ((DEBUG_WARN, "Ufs: GCC branch 0x%x still halted\n", Offset));
}

/*
 * UFS power domain. GDSCR bit 0 is SW_COLLAPSE: clearing it requests power on.
 * The block is not votable on this SoC, so a bounded poll is enough.
 */
STATIC
VOID
UfsPowerOnDomain (
  VOID
  )
{
  UINT32  Value;
  UINTN   Spin;

  Value  = MmioRead32 (GCC_BASE + GCC_UFS_GDSCR);
  Value &= ~BIT0;
  MmioWrite32 (GCC_BASE + GCC_UFS_GDSCR, Value);

  for (Spin = 0; Spin < 1000; Spin++) {
    if ((MmioRead32 (GCC_BASE + GCC_UFS_GDSCR) & BIT0) == 0) {
      return;
    }
    MicroSecondDelay (10);
  }

  DEBUG ((DEBUG_WARN, "Ufs: UFS power domain did not report on\n"));
}

STATIC
VOID
UfsGccBringUp (
  VOID
  )
{
  UINT32  Value;

  /* Keep the UFS block out of reset until clocks and power are stable. */
  MmioWrite32 (GCC_BASE + GCC_UFS_BCR, BIT0);
  MicroSecondDelay (1000);

  UfsPowerOnDomain ();
  UfsEnableGccBranch (GCC_UFS_CLKREF_CBCR);
  UfsEnableGccBranch (GCC_UFS_PHY_AUX_CBCR);
  UfsEnableGccBranch (GCC_UFS_UNIPRO_CBCR);
  UfsEnableGccBranch (GCC_UFS_ICE_CBCR);
  UfsEnableGccBranch (GCC_UFS_AXI_CBCR);
  UfsEnableGccBranch (GCC_UFS_AHB_CBCR);
  UfsEnableGccBranch (GCC_UFS_TX_SYMBOL_CBCR);
  UfsEnableGccBranch (GCC_UFS_RX_SYMBOL_CBCR);
  UfsEnableGccBranch (GCC_AGGRE2_UFS_AXI_CBCR);

  /* Deassert UFS block reset. */
  Value = MmioRead32 (GCC_BASE + GCC_UFS_BCR);
  MmioWrite32 (GCC_BASE + GCC_UFS_BCR, (UINT32)(Value & ~BIT0));
  MicroSecondDelay (1000);
}

STATIC
EFI_STATUS
UfsHciEnable (
  IN UINTN  HcBase
  )
{
  MmioWrite32 (HcBase + UFS_HC_IE, 0);
  MmioWrite32 (HcBase + UFS_HC_IS, 0xFFFFFFFFU);
  MmioWrite32 (HcBase + UFS_HC_HCE, 1);

  return UfsPollHcs (HcBase, UFS_HCS_UTRLRDY | UFS_HCS_UTMRLRDY | UFS_HCS_UCRDY,
                     UFS_HCS_UTRLRDY | UFS_HCS_UTMRLRDY | UFS_HCS_UCRDY,
                     UFS_HCS_TIMEOUT_US);
}

STATIC
EFI_STATUS
UfsLinkStartup (
  IN UINTN  HcBase
  )
{
  EFI_STATUS  Status;
  UINT32      Result;
  UINTN       Spin;

  Status = UfsSendUic (HcBase, UFS_UIC_DME_LINKSTARTUP, 0, 0, 0, &Result);
  if (EFI_ERROR (Status)) {
    UfsLog ("DME_LINKSTARTUP failed: %r\n", Status);
    return Status;
  }

  /* DME_LINKSTARTUP completion result carries the UniPro link status. */
  for (Spin = 0; Spin < UFS_LINK_TIMEOUT_MS; Spin++) {
    if (UfsPollHcs (HcBase, UFS_HCS_UCRDY, UFS_HCS_UCRDY, UFS_HCS_TIMEOUT_US) == EFI_SUCCESS) {
      break;
    }
    MicroSecondDelay (1000);
  }

  if ((Result & 0xFFFF) != 0) {
    UfsLog ("link startup status 0x%x (continuing)\n", Result & 0xFFFF);
  }

  return EFI_SUCCESS;
}

/*
 * Fill a UTRD, ring the doorbell and wait for completion.
 *
 * @param Flags         UPIU flags (0 for a query request).
 * @param DataDir       UFS_UTP_* value describing the data phase.
 * @param Prd           Optional PRD table already placed at Ucd + 1024.
 * @param PrdBytes      Byte size of the PRD table.
 */
STATIC
EFI_STATUS
UfsRunUtrd (
  IN UFS_PRIVATE  *Priv,
  IN UINT8        Flags,
  IN UINT32       DataDir,
  IN UINT16       PrdBytes
  )
{
  UFS_UTRD  *Utrd;
  UINTN     HcBase;
  UINTN     Elapsed;

  Utrd   = &Priv->Slot->Utrd;
  HcBase = Priv->HcBase;

  ZeroMem (Utrd, sizeof (*Utrd));
  Utrd->Header.Interrupt     = 1;
  Utrd->Header.DataDirection = DataDir;
  Utrd->Header.CommandType   = UFS_UTP_CMD_TYPE_STORAGE;
  Utrd->Header.Ocs           = UFS_OCS_INVALID;
  Utrd->CommandDescBaseAddr  = (UINT64)(UINTN)Priv->Slot->Ucd;
  Utrd->ResponseUpiuLength   = UFS_UPIU_ALIGNED_SIZE;
  Utrd->ResponseUpiuOffset   = UFS_UPIU_ALIGNED_SIZE;
  Utrd->PrdTableLength       = PrdBytes;
  Utrd->PrdTableOffset       = UFS_UPIU_ALIGNED_SIZE * 2;

  WriteBackInvalidateDataCacheRange (Priv->Slot, sizeof (*Priv->Slot));

  MmioWrite32 (HcBase + UFS_HC_IS, UFS_IS_UTRCS);
  MmioWrite32 (HcBase + UFS_HC_UTRLDBR, BIT0);

  for (Elapsed = 0; Elapsed < UFS_CMD_TIMEOUT_MS; Elapsed++) {
    if ((MmioRead32 (HcBase + UFS_HC_IS) & UFS_IS_UTRCS) != 0) {
      MmioWrite32 (HcBase + UFS_HC_IS, UFS_IS_UTRCS);
      InvalidateDataCacheRange (Priv->Slot, sizeof (*Priv->Slot));
      if (Utrd->Header.Ocs != UFS_OCS_SUCCESS) {
        UfsLog ("UTRD OCS 0x%x (flags 0x%x)\n", Utrd->Header.Ocs, Flags);
        return EFI_DEVICE_ERROR;
      }
      return EFI_SUCCESS;
    }
    MicroSecondDelay (1000);
  }

  UfsLog ("UTRD timeout (flags 0x%x)\n", Flags);
  return EFI_TIMEOUT;
}

/*
 * READ_DESCRIPTOR query. The descriptor payload arrives in the response UPIU
 * data segment, so no PRD is used.
 */
STATIC
EFI_STATUS
UfsReadDescriptor (
  IN  UFS_PRIVATE  *Priv,
  IN  UINT8        DescriptorIdn,
  IN  UINT8        Index,
  IN  UINT16       Selector,
  OUT VOID         *Buffer,
  IN  UINT32       Length
  )
{
  UFS_UPIU_QUERY_REQ  *Req;
  UFS_UPIU_RESPONSE   *Rsp;
  EFI_STATUS          Status;
  UINT32              Mps;

  Req = (UFS_UPIU_QUERY_REQ *)Priv->Slot->Ucd;
  Rsp = (UFS_UPIU_RESPONSE *)(Priv->Slot->Ucd + UFS_UPIU_ALIGNED_SIZE);

  ZeroMem (Priv->Slot->Ucd, UFS_UPIU_ALIGNED_SIZE * 2);

  Req->Header.TransactionCode = UFS_UPIU_TRANSACTION_QUERY_REQ;
  Req->Header.Flags           = 0;
  Req->Header.Lun             = 0;
  Req->Header.TaskTag         = 0;
  Req->Header.Function        = UFS_UPIU_QUERY_FUNC_READ_DESC;
  Req->Header.DataSegmentLength = (UINT16)Length;

  Req->Query.Opcode   = DescriptorIdn;
  Req->Query.Index    = Index;
  Req->Query.Selector = Selector;
  Req->Query.Length   = Length;

  Status = UfsRunUtrd (Priv, 0, UFS_UTP_DEVICE_TO_HOST, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Rsp->Header.Response != 0) {
    UfsLog ("descriptor 0x%x response 0x%x status 0x%x\n",
            DescriptorIdn, Rsp->Header.Response, Rsp->Header.Status);
    return EFI_DEVICE_ERROR;
  }

  Mps = (UINT32)Rsp->Header.DataSegmentLength;
  if (Mps > Length) {
    Mps = Length;
  }
  if (Mps == 0) {
    return EFI_DEVICE_ERROR;
  }

  CopyMem (Buffer, Rsp->Data, Mps);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
UfsScsiRw (
  IN UFS_PRIVATE  *Priv,
  IN BOOLEAN      Write,
  IN UINT64       Lba,
  IN UINT32       Blocks,
  IN OUT VOID     *Buffer
  )
{
  UFS_UPIU_COMMAND  *Cmd;
  UFS_PRD_ENTRY     *Prd;
  UINT32            Bytes;
  UINT32            PrdIndex;
  UINT32            PrdBytes;
  UINT64            Offset;
  UINT8             *Cdb;

  Bytes = Blocks * Priv->BlockSize;
  if ((Bytes == 0) || (Bytes > UFS_BOUNCE_BYTES) || (Bytes % Priv->BlockSize) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (Lba > Priv->LastLba || (Lba + Blocks - 1) > Priv->LastLba) {
    return EFI_INVALID_PARAMETER;
  }

  if (Write) {
    CopyMem (Priv->Bounce, Buffer, Bytes);
    WriteBackInvalidateDataCacheRange (Priv->Bounce, Bytes);
  } else {
    InvalidateDataCacheRange (Priv->Bounce, Bytes);
  }

  Cmd = (UFS_UPIU_COMMAND *)Priv->Slot->Ucd;
  ZeroMem (Priv->Slot->Ucd, UFS_UPIU_ALIGNED_SIZE * 2);

  Cmd->Header.TransactionCode = UFS_UPIU_TRANSACTION_COMMAND;
  Cmd->Header.Flags           = Write ? UFS_UPIU_FLAGS_WRITE : UFS_UPIU_FLAGS_READ;
  Cmd->Header.Lun             = 0;
  Cmd->Header.TaskTag         = 0;
  Cmd->Header.Function        = UFS_CMD_SET_TYPE_SCSI;
  Cmd->Header.DataSegmentLength = (UINT16)Bytes;
  Cmd->ExpectedDataTransferLength = Bytes;

  Cdb = Cmd->Cdb;
  if (Write) {
    Cdb[0] = 0x2A; /* WRITE(10) */
  } else {
    Cdb[0] = 0x28; /* READ(10)  */
  }
  Cdb[2] = (UINT8)(Lba >> 24);
  Cdb[3] = (UINT8)(Lba >> 16);
  Cdb[4] = (UINT8)(Lba >> 8);
  Cdb[5] = (UINT8)(Lba);
  Cdb[7] = (UINT8)(Blocks >> 8);
  Cdb[8] = (UINT8)(Blocks);

  Prd      = (UFS_PRD_ENTRY *)(Priv->Slot->Ucd + UFS_UPIU_ALIGNED_SIZE * 2);
  PrdIndex = 0;
  for (Offset = 0; Offset < Bytes; Offset += EFI_PAGE_SIZE) {
    UINT32  Chunk;

    Chunk = Bytes - (UINT32)Offset;
    if (Chunk > EFI_PAGE_SIZE) {
      Chunk = EFI_PAGE_SIZE;
    }
    Prd[PrdIndex].Addr     = (UINT64)(UINTN)(Priv->Bounce + Offset);
    Prd[PrdIndex].Reserved = 0;
    Prd[PrdIndex].Size     = Chunk;
    PrdIndex++;
  }

  PrdBytes = PrdIndex * sizeof (UFS_PRD_ENTRY);
  if (UfsRunUtrd (Priv, Cmd->Header.Flags,
                  Write ? UFS_UTP_HOST_TO_DEVICE : UFS_UTP_DEVICE_TO_HOST,
                  (UINT16)PrdBytes) != EFI_SUCCESS) {
    return EFI_DEVICE_ERROR;
  }

  if (!Write) {
    InvalidateDataCacheRange (Priv->Bounce, Bytes);
    CopyMem (Buffer, Priv->Bounce, Bytes);
  }

  return EFI_SUCCESS;
}

/* ---- EFI_BLOCK_IO_PROTOCOL ----------------------------------------- */

STATIC
EFI_STATUS
EFIAPI
UfsReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  OUT VOID                  *Buffer
  )
{
  UFS_PRIVATE  *Priv;
  UINT32       Blocks;
  UINT32       Done;
  UINT8        *Cursor;

  Priv = &mUfs;
  if ((Buffer == NULL) || (BufferSize == 0) || (BufferSize % Priv->BlockSize) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (MediaId != Priv->Media.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  Blocks = (UINT32)(BufferSize / Priv->BlockSize);
  Cursor = Buffer;
  for (Done = 0; Done < Blocks; Done += UFS_BOUNCE_BYTES / Priv->BlockSize) {
    UINT32  Chunk;

    Chunk = Blocks - Done;
    if (Chunk > UFS_BOUNCE_BYTES / Priv->BlockSize) {
      Chunk = UFS_BOUNCE_BYTES / Priv->BlockSize;
    }
    if (EFI_ERROR (UfsScsiRw (Priv, FALSE, Lba + Done, Chunk, Cursor))) {
      return EFI_DEVICE_ERROR;
    }
    Cursor += (UINTN)Chunk * Priv->BlockSize;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UfsWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  UFS_PRIVATE  *Priv;
  UINT32       Blocks;
  UINT32       Done;
  UINT8        *Cursor;

  Priv = &mUfs;
  if ((Buffer == NULL) || (BufferSize == 0) || (BufferSize % Priv->BlockSize) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  if (MediaId != Priv->Media.MediaId) {
    return EFI_MEDIA_CHANGED;
  }
  if (Priv->Media.ReadOnly) {
    return EFI_WRITE_PROTECTED;
  }

  Blocks = (UINT32)(BufferSize / Priv->BlockSize);
  Cursor = Buffer;
  for (Done = 0; Done < Blocks; Done += UFS_BOUNCE_BYTES / Priv->BlockSize) {
    UINT32  Chunk;

    Chunk = Blocks - Done;
    if (Chunk > UFS_BOUNCE_BYTES / Priv->BlockSize) {
      Chunk = UFS_BOUNCE_BYTES / Priv->BlockSize;
    }
    if (EFI_ERROR (UfsScsiRw (Priv, TRUE, Lba + Done, Chunk, Cursor))) {
      return EFI_DEVICE_ERROR;
    }
    Cursor += (UINTN)Chunk * Priv->BlockSize;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UfsFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
UfsReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

/* ---- driver entry -------------------------------------------------- */

STATIC
EFI_STATUS
UfsBringUp (
  IN UFS_PRIVATE  *Priv
  )
{
  EFI_STATUS          Status;
  UFS_UNIT_DESCRIPTOR Unit;
  UINT64              BlockCount;
  UINT8               Index;
  UINT8               Device[0x40];
  UINTN               HcBase;

  HcBase      = (UINTN)UFS_HC_BASE;
  Priv->HcBase = HcBase;

  Priv->Slot = AllocateAlignedPages (EFI_SIZE_TO_PAGES (sizeof (UFS_COMMAND_SLOT)), EFI_PAGE_SIZE);
  Priv->Bounce = AllocateAlignedPages (EFI_SIZE_TO_PAGES (UFS_BOUNCE_BYTES), EFI_PAGE_SIZE);
  if ((Priv->Slot == NULL) || (Priv->Bounce == NULL)) {
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem (Priv->Slot, sizeof (UFS_COMMAND_SLOT));

  UfsGccBringUp ();

  if (EFI_ERROR (UfsHciEnable (HcBase))) {
    UfsLog ("controller did not become ready (CAP=0x%x)\n", MmioRead32 (HcBase + UFS_HC_CAP));
    return EFI_NOT_FOUND;
  }

  UfsLog ("UFS version 0x%x, CAP 0x%x\n",
          MmioRead32 (HcBase + UFS_HC_VER), MmioRead32 (HcBase + UFS_HC_CAP));

  /* Single request list entry, task tag 0. */
  MmioWrite32 (HcBase + UFS_HC_UTRLBA, (UINT32)(UINTN)&Priv->Slot->Utrd);
  MmioWrite32 (HcBase + UFS_HC_UTRLBAU, (UINT32)((UINT64)(UINTN)&Priv->Slot->Utrd >> 32));
  MmioWrite32 (HcBase + UFS_HC_UTRLRSR, 1);

  Status = UfsLinkStartup (HcBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ZeroMem (Device, sizeof (Device));
  Status = UfsReadDescriptor (Priv, UFS_DESC_IDN_DEVICE, 0, 0, Device, sizeof (Device));
  if (EFI_ERROR (Status)) {
    UfsLog ("device descriptor read failed: %r\n", Status);
    return Status;
  }
  UfsLog ("UFS device: LUs=%u boot=%u spec=0x%x\n",
          Device[6], Device[8], (UINT32)((Device[16] << 8) | Device[17]));

  ZeroMem (&Unit, sizeof (Unit));
  Status = UfsReadDescriptor (Priv, UFS_DESC_IDN_UNIT, 0, 0, &Unit, sizeof (Unit));
  if (EFI_ERROR (Status)) {
    UfsLog ("unit descriptor read failed: %r\n", Status);
    return Status;
  }

  if (Unit.LuEnable == 0) {
    UfsLog ("LUN 0 is not enabled\n");
    return EFI_NOT_FOUND;
  }

  Priv->BlockSize = 1U << Unit.LogicalBlockSize;
  if ((Priv->BlockSize < 512) || (Priv->BlockSize > 4096)) {
    UfsLog ("unsupported logical block size shift %u\n", Unit.LogicalBlockSize);
    return EFI_UNSUPPORTED;
  }

  BlockCount = 0;
  for (Index = 0; Index < 8; Index++) {
    BlockCount = (BlockCount << 8) | Unit.LogicalBlockCount[Index];
  }
  if (BlockCount == 0) {
    UfsLog ("unit descriptor reports zero blocks\n");
    return EFI_NOT_FOUND;
  }

  Priv->LastLba = BlockCount - 1;
  UfsLog ("LUN 0: %lx blocks of %u bytes\n", BlockCount, Priv->BlockSize);

  Priv->Media.MediaId          = 0x4A41534E; /* "JASN" */
  Priv->Media.RemovableMedia   = FALSE;
  Priv->Media.MediaPresent     = TRUE;
  Priv->Media.LogicalPartition = FALSE;
  Priv->Media.ReadOnly         = FALSE;
  Priv->Media.WriteCaching     = FALSE;
  Priv->Media.BlockSize        = Priv->BlockSize;
  Priv->Media.IoAlign          = 0;
  Priv->Media.LastBlock        = Priv->LastLba;

  Priv->BlockIo.Revision   = EFI_BLOCK_IO_PROTOCOL_REVISION;
  Priv->BlockIo.Media      = &Priv->Media;
  Priv->BlockIo.Reset      = UfsReset;
  Priv->BlockIo.ReadBlocks = UfsReadBlocks;
  Priv->BlockIo.WriteBlocks = UfsWriteBlocks;
  Priv->BlockIo.FlushBlocks = UfsFlushBlocks;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UfsDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  VENDOR_DEVICE_PATH        *Vendor;
  EFI_DEVICE_PATH_PROTOCOL  *End;

  if (!FeaturePcdGet (PcdUfsEnabled)) {
    DEBUG ((DEBUG_INFO, "Ufs: disabled by PCD\n"));
    return EFI_UNSUPPORTED;
  }

  ZeroMem (&mUfs, sizeof (mUfs));

  Status = UfsBringUp (&mUfs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Ufs: no block device (%r); boot continues\n", Status));
    return EFI_SUCCESS;
  }

  Vendor = AllocateZeroPool (sizeof (VENDOR_DEVICE_PATH) + sizeof (EFI_DEVICE_PATH_PROTOCOL));
  if (Vendor == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Vendor->Header.Type    = HARDWARE_DEVICE_PATH;
  Vendor->Header.SubType = HW_VENDOR_DP;
  SetDevicePathNodeLength (&Vendor->Header, sizeof (VENDOR_DEVICE_PATH));
  CopyGuid (&Vendor->Guid, &gEfiCallerIdGuid);

  End = (EFI_DEVICE_PATH_PROTOCOL *)(Vendor + 1);
  End->Type    = END_DEVICE_PATH_TYPE;
  End->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  SetDevicePathNodeLength (End, sizeof (EFI_DEVICE_PATH_PROTOCOL));

  DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)Vendor;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mUfs.Handle,
                  &gEfiDevicePathProtocolGuid, DevicePath,
                  &gEfiBlockIoProtocolGuid, &mUfs.BlockIo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Ufs: failed to publish block I/O: %r\n", Status));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "Ufs: LUN 0 published as block device\n"));
  return EFI_SUCCESS;
}
