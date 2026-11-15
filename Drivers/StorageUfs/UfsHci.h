/*
 * UFS Host Controller Interface 2.1 definitions used by the jason driver.
 *
 * Register offsets, UTRD/UPIU layouts and OCS values follow the UFS HCI 2.1
 * specification. The layout was cross-checked against the mainline Linux
 * ufshci.h/ufshcd.c definitions so that the fields are not guessed.
 */
#pragma once

#include <Uefi.h>

/* ---- HCI register map (UFS HCI 2.x) -------------------------------- */
#define UFS_HC_CAP                0x00
#define UFS_HC_VER                0x08
#define UFS_HC_IS                 0x20
#define UFS_HC_IE                 0x24
#define UFS_HC_HCS                0x30
#define UFS_HC_HCE                0x34
#define UFS_HC_UTRLBA             0x50
#define UFS_HC_UTRLBAU            0x54
#define UFS_HC_UTRLDBR            0x58
#define UFS_HC_UTRLCLR            0x5C
#define UFS_HC_UTRLRSR            0x60
#define UFS_HC_UTMRLBA            0x70
#define UFS_HC_UTMRLBAU           0x74
#define UFS_HC_UTMRLDBR           0x78
#define UFS_HC_UICCMD             0x90
#define UFS_HC_UCMDARG1           0x94
#define UFS_HC_UCMDARG2           0x98
#define UFS_HC_UCMDARG3           0x9C

/* HCS bits */
#define UFS_HCS_UTRLRDY           BIT1
#define UFS_HCS_UTMRLRDY          BIT2
#define UFS_HCS_UCRDY             BIT3

/* IS bits */
#define UFS_IS_UTRCS              BIT0
#define UFS_IS_UTMRCS             BIT9
#define UFS_IS_UCCS               BIT10

/* ---- UIC command opcodes ------------------------------------------ */
#define UFS_UIC_DME_GET           0x01
#define UFS_UIC_DME_SET           0x02
#define UFS_UIC_DME_LINKSTARTUP   0x16

#define UFS_UIC_ARG_MIB(attr)     ((UINT32)(attr) << 16)

/* ---- UTRD --------------------------------------------------------- */
#define UFS_UTP_CMD_TYPE_STORAGE  0x1
#define UFS_UTP_NO_DATA           0x0
#define UFS_UTP_HOST_TO_DEVICE    0x1
#define UFS_UTP_DEVICE_TO_HOST    0x2
#define UFS_OCS_INVALID           0xFF
#define UFS_OCS_SUCCESS           0x00

#pragma pack(1)
typedef struct {
  UINT8     Cci;
  UINT8     EhsLength;
  UINT8     EnableCrypto : 1;
  UINT8     Reserved2    : 7;
  UINT8     Interrupt    : 1;
  UINT8     DataDirection : 2;
  UINT8     Reserved1    : 1;
  UINT8     CommandType  : 4;
  UINT32    Dunl;
  UINT8     Ocs;
  UINT8     Cds;
  UINT16    Ldbc;
  UINT32    Dunu;
} UFS_UTRD_HEADER;

typedef struct {
  UFS_UTRD_HEADER    Header;               /* 0x00, 16 bytes */
  UINT64             CommandDescBaseAddr;  /* 0x10 */
  UINT16             ResponseUpiuLength;   /* 0x18 */
  UINT16             ResponseUpiuOffset;   /* 0x1A */
  UINT16             PrdTableLength;       /* 0x1C */
  UINT16             PrdTableOffset;       /* 0x1E */
} UFS_UTRD;                                /* 32 bytes */

typedef struct {
  UINT64             Addr;
  UINT32             Reserved;
  UINT32             Size;
} UFS_PRD_ENTRY;                           /* 16 bytes */

/* ---- UPIU --------------------------------------------------------- */
#define UFS_UPIU_ALIGNED_SIZE     512
#define UFS_UPIU_CDB_SIZE         16
#define UFS_QUERY_OSF_SIZE        8

#define UFS_UPIU_TRANSACTION_COMMAND   0x01
#define UFS_UPIU_TRANSACTION_QUERY_REQ 0x16

#define UFS_UPIU_FLAGS_NONE       0x00
#define UFS_UPIU_FLAGS_READ       0x40
#define UFS_UPIU_FLAGS_WRITE      0x20

#define UFS_UPIU_QUERY_FUNC_READ_DESC  0x01
#define UFS_UPIU_QUERY_FUNC_WRITE_DESC 0x02

#define UFS_CMD_SET_TYPE_SCSI     0x00

/* Descriptor IDs used as the query "opcode" for descriptor reads. */
#define UFS_DESC_IDN_DEVICE       0x00
#define UFS_DESC_IDN_CONFIG       0x01
#define UFS_DESC_IDN_UNIT         0x02
#define UFS_DESC_IDN_GEOMETRY     0x07

typedef struct {
  UINT8     TransactionCode;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;
  UINT8     InitiatorId;
  UINT8     Function;   /* command set type or query function */
  UINT8     Response;
  UINT8     Status;
  UINT8     EhsLength;
  UINT8     DeviceInformation;
  UINT16    DataSegmentLength;
} UFS_UPIU_HEADER;                            /* 12 bytes */

typedef struct {
  UFS_UPIU_HEADER Header;
  UINT32          ExpectedDataTransferLength;
  UINT8           Cdb[UFS_UPIU_CDB_SIZE];
} UFS_UPIU_COMMAND;                           /* 32 bytes */

typedef struct {
  UINT8     Opcode;     /* descriptor ID for READ_DESCRIPTOR */
  UINT8     Idn;
  UINT8     Index;
  UINT8     Selector;
  UINT16    ReservedOsf;
  UINT16    Reserved2;
  UINT32    Length;
  UINT32    Value;
} UFS_UPIU_QUERY;

typedef struct {
  UFS_UPIU_HEADER Header;
  UFS_UPIU_QUERY  Query;
} UFS_UPIU_QUERY_REQ;                         /* 32 bytes */

typedef struct {
  UFS_UPIU_HEADER Header;
  UINT8           Data[UFS_UPIU_ALIGNED_SIZE - sizeof (UFS_UPIU_HEADER)];
} UFS_UPIU_RESPONSE;                          /* 512 bytes */

/* Unit descriptor fields we depend on (UFS 2.1, table "Unit Descriptor"). */
typedef struct {
  UINT8     Length;
  UINT8     DescriptorIdn;
  UINT8     UnitIndex;
  UINT8     LuEnable;
  UINT8     BootLunId;
  UINT8     LuWriteProtect;
  UINT8     MemoryType;
  UINT8     DataReliability;
  UINT8     LogicalBlockSize;
  UINT8     LogicalBlockCount[8];
  UINT8     ProvisioningType;
} UFS_UNIT_DESCRIPTOR;
#pragma pack()

/* One command slot; the controller only needs task tag 0 for a blocking driver. */
typedef struct {
  UFS_UTRD         Utrd;
  UINT8            Ucd[UFS_UPIU_ALIGNED_SIZE * 2 + sizeof (UFS_PRD_ENTRY)];
} UFS_COMMAND_SLOT;
