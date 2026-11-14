/*
 * TouchI2c.c — I2C SLIMBUS touch controller driver (Xiaomi Note 3)
 *
 * The jason touch panel uses a TrueTouch Classic TSPP4xxx or equivalent
 * slave at I2C address 0x38, connected through QUP#2's I2C controller.
 * The controller is shared between the touchscreen (SLIMBUS) and the
 * ambient-light sensor (ALS). This driver only talks to the touch IC.
 *
 * Register base: QUP_I2C1_BASE = 0x164C0000ULL
 * Reference:
 *   - linux/kernel/msm/drivers/i2c/i2c-qcom-cdc.c
 *   - SDM660 TRM §9 "Quartz Universal Peripheral (QUP) I2C"
 */

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../UefiPlatform/Include/Hardware/Platform.h"

#define TOUCH_SLAVE_ADDR      0x38          /* 7-bit I2C address of touch IC    */
#define TOUCH_REG_XY          0x04          /* register for first X/Y pair       */
#define TOUCH_REG_STATUS      0x00          /* status register                   */
#define TOUCH_REG_NUM_TOUCH   0x02          /* number of simultaneous touches    */
#define I2C_TIMEOUT_US        5000           /* 5 ms per transaction              */

/* ─── QUP v2 I2C register layout ────────────────────────────── */
#define I2C_MSCMD         0x00            /* master sequence command            */
#define I2C_MSTATUS       0x04            /* master sequence status             */
#define I2C_MSCTRL        0x08            /* master sequence control            */
#define I2C_RXFIFO        0x0C            /* receive data FIFO                  */
#define I2C_TXFIFO        0x10            /* transmit data FIFO                 */
#define I2C_CLK_RT        0x14            /* clock ratio (SCL = PCLK / ratio)   */
#define I2C_MISTATUS      0x18            /* IRQ status                         */
#define I2C_MIE_MASK      0x1C            /* IRQ enable mask                    */

/* ─── Master sequence commands ───────────────────────────────── */
#define I2C_CMD_START         0x01        /* generate START condition           */
#define I2C_CMD_WRITE_ADDR    0x02        /* write device address + R/W bit     */
#define I2C_CMD_READ_ADDR     0x03        /* read device address                */
#define I2C_CMD_WRITE_DATA    0x04        /* write next byte from TX FIFO       */
#define I2C_CMD_READ_DATA     0x05        /* read next byte into RX FIFO        */
#define I2C_CMD_STOP          0x06        /* generate STOP condition            */
#define I2C_CMD_RESTART       0x07        /* generate repeated START            */

static volatile UINT32 * const I2C = (volatile UINT32 *)QUP_I2C1_BASE;

/* ─── Helper: wait until hardware is idle ──────────────────────── */
STATIC
EFI_STATUS
WaitIdle (
  VOID
  )
{
  for (UINTN t = 0; t < 20000; ++t) {
    if ((READ_REGISTER_UINT32 (I2C + I2C_MSTATUS) & BIT (0)) != 0)
      return EFI_SUCCESS;  /* MST_IDLE state reached */
    MicroSecondDelay (1);
  }
  return EFI_TIMEOUT;
}

/* ─── Public API ───────────────────────────────────────────────── */

/**
 * TouchReadRegs — send an I2C READ transaction to query the touch IC.
 *
 * Issues a START → WRITE(SLAVE_ADDR+R/W=0) → WRITE(REG_ADDR) → REPEAT_START
 * → READ(SLAVE_ADDR+R/W=1) → READ(N bytes) → STOP sequence.
 *
 * Buffer must hold at least Len bytes. Returns EFI_SUCCESS on completion.
 */
EFI_STATUS
EFIAPI
TouchReadRegs (
  IN  UINT8   RegAddr,
  OUT UINT8  *Buffer,
  IN  UINTN   Len
  )
{
  if (!Buffer || Len == 0)
    return EFI_INVALID_PARAMETER;

  /* Phase 1 — issue START */
  WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_START);
  EFI_STATUS s = WaitIdle(); if (EFI_ERROR(s)) return s;

  /* Phase 2 — write device address (WR direction) */
  WRITE_REGISTER_UINT32 (I2C + I2C_TXFIFO, (TOUCH_SLAVE_ADDR << 1) | 0x0);  /* R/W = 0 */
  WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_WRITE_ADDR);
  s = WaitIdle(); if (EFI_ERROR(s)) return s;

  /* Phase 3 — write register address (internal pointer) */
  WRITE_REGISTER_UINT32 (I2C + I2C_TXFIFO, RegAddr);
  WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_WRITE_DATA);
  s = WaitIdle(); if (EFI_ERROR(s)) return s;

  /* Phase 4 — repeated START (read direction) */
  WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_RESTART);
  s = WaitIdle(); if (EFI_ERROR(s)) return s;

  /* Phase 5 — read device address (RD direction) */
  WRITE_REGISTER_UINT32 (I2C + I2C_TXFIFO, (TOUCH_SLAVE_ADDR << 1) | 0x1);
  WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_READ_ADDR);
  s = WaitIdle(); if (EFI_ERROR(s)) return s;

  /* Phase 6 – read N bytes; last byte gets NACK, others ACKed by HW */
  for (UINTN i = 0; i < Len; ++i) {
    BOOLEAN Last = (i == Len - 1);
    if (Last)
      WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_READ_DATA);
    else
      WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_READ_DATA);  /* ACK implicit */
    s = WaitIdle(); if (EFI_ERROR(s)) return s;

    Buffer[i] = (UINT8)(READ_REGISTER_UINT32 (I2C + I2C_RXFIFO) & 0xFF);
  }

  /* Phase 7 — STOP */
  WRITE_REGISTER_UINT32 (I2C + I2C_MSCMD, I2C_CMD_STOP);
  return WaitIdle();
}

/**
 * TouchGetPosition — query active touch points from the controller.
 *
 * Fills UpToPoints entries in OutPositions[]. Each entry has x/y in
 * raw 12-bit counts (typically 0..4095 for the jason panel).
 *
 * Returns the number of valid positions written, or 0 if no touch active.
 */
UINTN
EFIAPI
TouchGetPosition (
  OUT UINT16 *OutX,
  OUT UINT16 *OutY,
  IN  UINTN  MaxPoints
  )
{
  UINT8 StatusBuf[4];
  if (EFI_ERROR(TouchReadRegs (TOUCH_REG_STATUS, StatusBuf, sizeof(StatusBuf))))
    return 0;

  UINT8 NumTouches = (StatusBuf[1] >> 4) & 0x0F;   /* upper nibble of status byte */
  if (NumTouches == 0 || NumTouches > MaxPoints)
    NumTouches = MaxPoints;

  /* Read XY registers starting at offset 4 */
  for (UINTN i = 0; i < NumTouches; ++i) {
    UINT8 PosBuf[8];
    UINT8 regBase = TOUCH_REG_XY + i * 8;
    if (EFI_ERROR(TouchReadRegs (regBase, PosBuf, sizeof(PosBuf))))
      break;
    if (OutX) OutX[i] = ((UINT16)PosBuf[0] << 4) | (PosBuf[1] >> 4);
    if (OutY) OutY[i] = ((UINT16)(PosBuf[1] & 0x0F) << 8) | PosBuf[2];
  }

  return NumTouches;
}
