# 更新日志

本仓库只记录可复核的改动。任何"已实现"都不等于"已在真机验证"；运行状态一列在 `Docs/hardware-notes.md`。

## v0.4.0 — Windows 10 ARM64 外设描述（网卡 / USB / UFS）

### 新增

- `AcpiTables/ssdt_periph.asl` 发布三个设备节点：
  - `XHC0`，`_HID "PNP0D10"`，资源 `0x0A800000 / 0xC100` + GSI 163（DTB SPI 131 + 32）。这是 ACPI 标准 xHCI ID，Windows 10 ARM64 自带 USB 栈可直接绑定。
  - `WLAN`，`_HID "JAS0001"`（Jason 占位），资源 `0x18800000 / 0x800000`（DTB `qcom,icnss@18800000`）。
  - `UFS0`，`_HID "JAS0002"`（Jason 占位），资源 `0x01DA4000 / 0x3000`、`0x01DA7000 / 0xDB8`、`0x01DB0000 / 0x8000`。

### 说明

- 占位 `_HID` 的作用是让设备出现在固定 ACPI 路径并带上正确内存窗口，便于真实 Windows 10 ARM64 驱动按 INF 匹配绑定；没有驱动时设备可见但不可用。
- 替换方式：改 `_HID` 字符串，`_CRS` 不动。
- 驱动需为 ARM64 且签名；依赖 SMMU 的驱动还需 IORT，本平台暂未提供（ICNSS 在 DTB 中为 `qcom,smmu-s1-bypass`）。

### 文档

- `Docs/hardware-notes.md` 新增 8.7 节，写清 Windows 10 ARM64 侧的绑定步骤与限制。

## v0.3.0 — PMIC 总线与系统状态

### 新增

- `Drivers/SpmI`：SPMI PMIC arbiter（v2）总线主控。寄存器模型取自 `drivers/spmi/spmi-pmic-arb.c` 的 `pmic_arb_offset_v2` / `pmic_arb_fmt_cmd_v2` / `pmic_arb_find_apid`，地址取自 jason DTB `qcom,spmi@800f000`（core `0x0800F000`、chnls `0x08400000`、obsrvr `0x09400000`、cnfg `0x0800A000`，`qcom,ee = 0`）。对外发布 `gJasonSpmIProtocolGuid`，支持 1..8 字节读写，所有等待带超时。
- `Drivers/PmicKey`：PM660 PON `pon_1`（电源键）、`pon_2`（音量减）与 PM660L GPIO 7（音量加）状态读取，发布 `gJasonPmicKeyProtocolGuid`。
- `Drivers/PmicBattery`：PM660 FG gen3 电量读取（外设 `0x4000` + `0x09` 双字节影子寄存器，两字节一致才采信），发布 `gJasonBatteryProtocolGuid`。

### 修复

- `StorageUfs.inf`、`UsbHost.inf`、`TouchI2c.inf` 缺少 `[Includes] UefiPlatform/Include`，`#include <Hardware/Platform.h>` 会找不到头文件。
- `UefiPlatform.dec` 新增 `[Protocols]` 段，登记 SPMI / 按键 / 电量协议 GUID，并新增 `PcdPmicEnabled`。

### 文档

- `Docs/hardware-notes.md` 新增第 8 节：电量、电源键与音量键、音量输出、网络、相机的逐项现状，明确区分"固件能读"与"Windows 能显示"，并给出每一项在 Windows 侧缺什么驱动。
- README 状态表同步更新。

### 已知限制

- 电量只做百分比；电流/电压/温度/循环计数在 FG SRAM，需要先实现 FG memif。
- 充电状态未实现，SMB2 charger 寄存器映射尚未核实。
- 按键只在 UEFI 阶段可读；Windows 侧需要 OS 驱动把 SPMI 事件转成输入/电源事件。
- Wi-Fi（WCN3990/ICNSS）、音频（Tasha/WCD9341 over SLIMbus）、相机（CAMSS + CCI）没有 Windows 驱动，本轮不提供假节点。
- 全部代码未经真机启动验证，也未在本机跑过 EDK2 构建。

## v0.2.0 — 内部储存、USB 与触控

### 新增

- `Drivers/StorageUfs`：UFS HCI 2.x 块设备驱动。GCC 时钟/复位、GDSCR 上电、HCI 使能、`DME_LINKSTARTUP`、Device/Unit Descriptor 读取、`EFI_BLOCK_IO_PROTOCOL`（READ(10)/WRITE(10)/FLUSH/Reset）。
- `Drivers/UsbHost`：把 DWC3 xHCI 窗口（`0x0A800000`，长度 `0xC100`）注册为 non-discoverable PCI 设备；DSC/FDF 接入 `NonDiscoverablePciDeviceDxe`、`PciBusDxe`、`XhciDxe`、`UsbBusDxe`、`UsbMassStorageDxe`、`DiskIoDxe`、`PartitionDxe`、`EnhancedFatDxe`。
- `Drivers/TouchI2c`：TLMM GPIO 66/67 + QUP I2C tag 模式，复位后读取 Synaptics ID。

### 变更

- `Drivers/ClocksGpcd`、`Drivers/PmicRegulator` 收回为占位：原实现使用了按固定 stride 推算的 GCC/SPMI 偏移，与 SDM660 实际布局不符；PMIC 型号由 PMI632 纠正为 PM660/PM660L。

### 已知限制

- UFS 的 QMP PHY serdes 校准表依赖 bootloader，未重新训练，链路停在 PWM-G1；无 ICE/RPMB/多 LUN。
- 触控不发布 `EFI_ABSOLUTE_POINTER_PROTOCOL`，因为没有真实的 S3330 报文样本。

## v0.1.0 — DTB 提取与 GICv3

### 新增

- `Scripts/ExtractBootDtb.py`：纯 Python 扫描 boot.img 的 QCDT 区，按 root `model` 选出 jason 的 DTB，可选输出 DTS。
- `AcpiTables/madt_gen.c`：GICv3 MADT（GICD `0x17A00000` + GICR `0x17B00000` + 8×GICC）。
- `UefiPlatform/Include/Hardware/Platform.h`：按 DTB 核实的地址表。

### 修复

- UFS、USB、I2C、TLMM、GCC 的旧地址多处与 DTB 冲突，全部替换为 DTB 值并标注来源。
