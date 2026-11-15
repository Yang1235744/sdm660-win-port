/*
 * jason touch: TLMM + QUP I2C bring-up and Synaptics S3330 identification.
 *
 * Direct DTB evidence (synaptics_dsi_i2c@20 under i2c@c178000):
 *   compatible = "synaptics,dsx-i2c-force"
 *   reg        = 0x20
 *   ub-i2c-addr= 0x2C
 *   irq-gpio   = 67, reset-gpio = 66
 *
 * Status: bus and pin layer only. The driver pulses reset, performs one
 * bounded identification read and logs the result. It deliberately does not
 * publish EFI_ABSOLUTE_POINTER_PROTOCOL, because the S3330 report layout used
 * by this panel variant is not documented in the DTB and a guessed layout
 * would produce a non-functional pointer. Everything stays behind
 * PcdTouchEnabled.
 */

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Hardware/Platform.h>

/* QUP I2C register offsets, mainline drivers/i2c/busses/i2c-qup.c. */
#define QUP_CONFIG          0x000
#define QUP_STATE           0x004
#define QUP_IO_MODE         0x008
#define QUP_SW_RESET        0x00C
#define QUP_OPERATIONAL     0x018
#define QUP_ERROR_FLAGS     0x01C
#define QUP_ERROR_FLAGS_EN  0x020
#define QUP_OPERATIONAL_MASK 0x028
#define QUP_MX_OUTPUT_CNT   0x100
#define QUP_OUT_FIFO_BASE   0x110
#define QUP_MX_WRITE_CNT    0x150
#define QUP_MX_INPUT_CNT    0x200
#define QUP_MX_READ_CNT     0x208
#define QUP_IN_FIFO_BASE    0x218
#define QUP_I2C_CLK_CTL     0x400
#define QUP_I2C_STATUS      0x404

#define QUP_STATE_MASK      3
#define QUP_STATE_VALID     BIT2
#define QUP_RESET_STATE     0
#define QUP_RUN_STATE       1
#define QUP_PAUSE_STATE     3

#define QUP_OUTPUT_BLK_MODE (1 << 10)
#define QUP_INPUT_BLK_MODE  (1 << 12)
#define QUP_UNPACK_EN       BIT14
#define QUP_PACK_EN         BIT15

#define QUP_OUT_SVC_FLAG    BIT8
#define QUP_IN_SVC_FLAG     BIT9
#define QUP_MX_OUTPUT_DONE  BIT10
#define QUP_MX_INPUT_DONE   BIT11
#define QUP_I2C_NACK_FLAG   BIT3

#define QUP_TAG_V2_START        0x81
#define QUP_TAG_V2_DATAWR       0x82
#define QUP_TAG_V2_DATAWR_STOP  0x83
#define QUP_TAG_V2_DATARD       0x85
#define QUP_TAG_V2_DATARD_STOP  0x87

#define TOUCH_I2C_TIMEOUT_US    10000U
#define TOUCH_I2C_NACK_MSG_LEN  1800U

STATIC
VOID
TouchLog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  CHAR8    Buffer[160];
  VA_LIST  Marker;

  VA_START (Marker, Format);
  AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);

  DEBUG ((DEBUG_INFO, "Touch: %a\n", Buffer));
}

STATIC
VOID
TouchPinMux (
  IN UINT32  Gpio,
  IN UINT32  Config,
  IN BOOLEAN Output
  )
{
  UINT32  Value;

  Value  = MmioRead32 (TLMM_GPIO_CFG (Gpio));
  Value &= ~(TLMM_CFG_FUNC_MASK | 0x00000E00U | 0x000000C0U);
  Value |= TLMM_CFG_FUNC_GPIO | (Config & 0x00000EC0U);
  if (Output) {
    Value |= TLMM_CFG_DRV_8MA;
  }
  MmioWrite32 (TLMM_GPIO_CFG (Gpio), Value);
}

/*
 * QUP tag-mode block transfer. Tags are used so that a register address write
 * can be followed by a repeated start, which is what the S3330 expects.
 */
STATIC
EFI_STATUS
TouchI2cTransfer (
  IN UINT8   SlaveAddr,
  IN UINT8   *WriteTags,
  IN UINTN   WriteWords,
  OUT UINT8  *ReadTags,
  OUT UINT8  *ReadData,
  IN UINTN   ReadWords
  )
{
  UINTN  Base;
  UINTN  Index;
  UINTN  Elapsed;
  UINT32 Op;

  Base = (UINTN)QUP_TOUCH_I2C_BASE;

  /* Pause the block. */
  MmioWrite32 (Base + QUP_STATE, QUP_PAUSE_STATE);
  for (Elapsed = 0; Elapsed < TOUCH_I2C_TIMEOUT_US; Elapsed += 10) {
    if ((MmioRead32 (Base + QUP_STATE) & QUP_STATE_VALID) != 0) {
      break;
    }
    MicroSecondDelay (10);
  }

  Op = MmioRead32 (Base + QUP_IO_MODE);
  Op |= QUP_OUTPUT_BLK_MODE | QUP_UNPACK_EN | QUP_PACK_EN;
  if (ReadWords != 0) {
    Op |= QUP_INPUT_BLK_MODE;
  }
  MmioWrite32 (Base + QUP_IO_MODE, Op);

  MmioWrite32 (Base + QUP_MX_OUTPUT_CNT, (UINT32)WriteWords);
  MmioWrite32 (Base + QUP_MX_WRITE_CNT, (UINT32)WriteWords);
  if (ReadWords != 0) {
    MmioWrite32 (Base + QUP_MX_INPUT_CNT, (UINT32)ReadWords);
    MmioWrite32 (Base + QUP_MX_READ_CNT, (UINT32)ReadWords);
  }

  for (Index = 0; Index < WriteWords; Index++) {
    MmioWrite32 (Base + QUP_OUT_FIFO_BASE, WriteTags[Index]);
  }

  MmioWrite32 (Base + QUP_STATE, QUP_RUN_STATE);

  for (Elapsed = 0; Elapsed < TOUCH_I2C_TIMEOUT_US; Elapsed += 10) {
    UINT32  Status;

    Status = MmioRead32 (Base + QUP_OPERATIONAL);
    if ((Status & QUP_I2C_NACK_FLAG) != 0) {
      TouchLog ("NACK from slave 0x%x\n", SlaveAddr);
      return EFI_DEVICE_ERROR;
    }
    if (ReadWords == 0) {
      if ((Status & QUP_MX_OUTPUT_DONE) != 0) {
        break;
      }
    } else if ((Status & QUP_MX_INPUT_DONE) != 0) {
      break;
    }
    MicroSecondDelay (10);
  }

  if (Elapsed >= TOUCH_I2C_TIMEOUT_US) {
    TouchLog ("QUP timeout, operational 0x%x\n", MmioRead32 (Base + QUP_OPERATIONAL));
    return EFI_TIMEOUT;
  }

  if (ReadWords != 0) {
    for (Index = 0; Index < ReadWords; Index++) {
      UINT32  Word;

      Word = MmioRead32 (Base + QUP_IN_FIFO_BASE);
      if (ReadData != NULL) {
        ReadData[Index] = (UINT8)(Word & 0xFF);
      }
    }
  }

  MmioWrite32 (Base + QUP_STATE, QUP_PAUSE_STATE);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TouchReadRegister (
  IN  UINT8  SlaveAddr,
  IN  UINT8  Register,
  OUT UINT8  *Data,
  IN  UINTN  Length
  )
{
  UINT8  Tags[8];
  UINT8  ReadTags[8];

  if (Length == 0 || Length > 8) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (ReadTags, sizeof (ReadTags));
  Tags[0] = QUP_TAG_V2_START;
  Tags[1] = (UINT8)(SlaveAddr << 1);
  Tags[2] = QUP_TAG_V2_DATAWR;
  Tags[3] = Register;
  if (Length == 1) {
    Tags[4] = QUP_TAG_V2_DATARD_STOP;
  } else {
    Tags[4] = QUP_TAG_V2_DATARD;
  }
  Tags[5] = (UINT8)((SlaveAddr << 1) | 1);
  if (Length > 1) {
    Tags[6] = QUP_TAG_V2_DATARD_STOP;
  }

  return TouchI2cTransfer (SlaveAddr,
                           Tags,
                           (Length == 1) ? 5 : 7,
                           ReadTags,
                           Data,
                           Length);
}

EFI_STATUS
EFIAPI
TouchDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT8       Id[6];

  if (!FeaturePcdGet (PcdTouchEnabled)) {
    DEBUG ((DEBUG_INFO, "Touch: disabled by PCD\n"));
    return EFI_UNSUPPORTED;
  }

  /* Reset line: open drain output, then release the panel. */
  TouchPinMux (TOUCH_RESET_GPIO, TLMM_CFG_PULL_UP, TRUE);
  TouchPinMux (TOUCH_IRQ_GPIO, TLMM_CFG_PULL_UP, FALSE);

  /* Drive reset low and back high using the CFG output register value. */
  MmioWrite32 (TLMM_GPIO_CFG (TOUCH_RESET_GPIO),
               MmioRead32 (TLMM_GPIO_CFG (TOUCH_RESET_GPIO)) & ~TLMM_CFG_PULL_UP);
  MicroSecondDelay (TOUCH_RESET_ACTIVE_MS * 1000);
  MmioWrite32 (TLMM_GPIO_CFG (TOUCH_RESET_GPIO),
               MmioRead32 (TLMM_GPIO_CFG (TOUCH_RESET_GPIO)) | TLMM_CFG_PULL_UP);
  MicroSecondDelay (TOUCH_RESET_DELAY_MS * 1000);

  ZeroMem (Id, sizeof (Id));
  Status = TouchReadRegister (TOUCH_I2C_SLAVE_ADDR, 0x00, Id, sizeof (Id));
  if (EFI_ERROR (Status)) {
    TouchLog ("identification read failed: %r\n", Status);
    return EFI_SUCCESS;
  }

  TouchLog ("ID %02x %02x %02x %02x %02x %02x\n",
            Id[0], Id[1], Id[2], Id[3], Id[4], Id[5]);

  /*
   * The report parser is intentionally absent: S3330 report offsets depend on
   * the panel firmware build, and guessing them would create a broken pointer
   * device. Capture one report with Android running before enabling input.
   */
  DEBUG ((DEBUG_WARN, "Touch: controller reachable, AbsolutePointer not published\n"));
  return EFI_SUCCESS;
}
