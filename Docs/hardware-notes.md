# jason 硬件数据与签名链记录

## 1. 当前结论

已取得一份 Xiaomi Mi Note 3（jason）原始 `boot.img`，并从其中唯一选出根 `model` 含 `Jason` 的完整 DTB。以下“DTB 已核实”仅表示值直接来自该匹配镜像的设备树，不表示驱动已经适配、真机 Linux 数据已交叉核对或 Windows 已启动。

| 项目 | 当前证据 | 当前处理 |
|---|---|---|
| PLATCTL 基址、解锁位 | 尚无对应节点、驱动语义或 TRM 证据 | 不填值；与 GPLL 锁定位完全分开 |
| GICD/GICR、GIC 版本 | `interrupt-controller@17a00000` 明确为 `arm,gic-v3` | MADT 改为 GICD + GICR + 8 个 GICC；删除 GICv2 GICC PCD |
| 各核 MPIDR | `/cpus` 的 8 个 `cpu@.../reg` | 与生成器数值一致；仍待真机运行验证 |
| UFS HC/PHY/ICE | DTB 有 compatible、寄存器窗口、时钟引用及中断 | 只记录资源；驱动入口继续返回 `EFI_UNSUPPORTED` |
| Wi-Fi/Bluetooth | `qcom,icnss@18800000`、`bt_wcn3990`、Slimbus `wcn3990` | 按 WCN3990/ICNSS 记录，不再假设 QCA6174 PCIe；驱动保持禁用 |
| ADC/PMIC | SPMI 仲裁器、PM660/PM660L、VADC/ADC-TM/RRADC 节点 | 地址解释为 SPMI 总线/外设偏移，不当作普通 flat MMIO |
| UART/QUP/TLMM/USB/触摸 | DTB 有对应节点和寄存器资源 | 记录直接证据；时钟、复位、电源和 pinctrl 未完整实现前不启用驱动 |
| UEFI 装载内存 | `reserved-memory` 有多段固定保留区及动态分配池 | FDF 的 `0xA0000000` 仍未证明安全，不生成可刷写结论 |

六个研究驱动入口均继续保持 `EFI_UNSUPPORTED`。本轮没有刷写设备、修改原始镜像或执行硬件 MMIO。

## 2. 镜像身份与提取结果

原始镜像以只读方式处理，身份和选中 DTB 如下：

| 项目 | 值 |
|---|---|
| boot.img SHA-256 | `36B023CCABB00CC654EA37D4DF517F60AE4A0E422553877F64BDA1D596B9847A` |
| Android boot header | v0；header 的 `dt_size=0`，DTB 位于内核附加的 QCDT 区域 |
| 原始 FDT magic 出现次数 | 115；magic 出现不代表每处都是完整有效 DTB |
| 当前脚本结构校验通过的 DTB | 32 |
| 选中 DTB 文件偏移 | `0x1BDDBBC` |
| 选中 DTB `totalsize` | `366521` 字节 |
| 根 `model` | `Qualcomm Technologies, Inc. SDM 660 PM660 + PM660L, Int Codec, Jason` |
| DTB SHA-256 | `360A02D75A7B57AD1D41F05FFC7C598ADC0BB25805AA96907DF6E54E7717C432` |
| DTS（LF 换行）SHA-256 | `E0528884EEFDC9102F562B393E5893F9A831093B44CA4BA0464273F968485715` |

选择依据是根节点的 `model`，不是文件偏移、QCDT 顺序或其他机型的相似寄存器值。上述哈希必须随证据一起保存，避免混入其他 ROM 的设备树。

## 3. 从 boot.img 提取 DTB/DTS

`Scripts/ExtractBootDtb.py` 使用 Python 标准库扫描完整镜像、校验 FDT 结构并读取根 `model`，不要求 `unpackbootimg` 或 `dtc`：

```bash
python3 Scripts/ExtractBootDtb.py \
  --boot-img /path/to/jason-boot.img \
  --output hardware/jason-boot
```

默认以不区分大小写的 `jason` 子串筛选根 `model`。匹配数不是 1 时脚本失败并列出原因，不会任意取一个 DTB。可用 `--model` 指定更严格的唯一匹配条件。

DTB 提取本身无第三方依赖。安装可选的 `pyfdt` 后，同一命令还会生成 `jason.dts`；未安装时保留有效 DTB 并明确提示跳过 DTS：

```bash
python3 -m pip install pyfdt
```

脚本只读取输入镜像，不重打包、不签名、不刷写。如果匹配镜像没有目标 DTB，应继续检查同版本 `vendor_boot.img`、DTBO 分区及 boot header；不要拿其他机型的 DTS 替代。

## 4. 匹配 DTB 的直接硬件证据

### 4.1 CPU 与 GICv3

- `/cpus`：`reg = 0, 1, 2, 3, 0x100, 0x101, 0x102, 0x103`，`enable-method = "psci"`。
- `/soc/interrupt-controller@17a00000`：`compatible = "arm,gic-v3"`。
- `reg = <0x17A00000 0x10000 0x17B00000 0x100000>`：GICD 为 64 KiB，GICR 区域为 1 MiB。
- `#redistributor-regions = <1>`，`redistributor-stride = <0 0x20000>`。

因此 `UefiPlatform/UefiPlatform.dsc` 使用：

```ini
gJasonPlatformTokenSpaceGuid.PcdGicDistributorBase|0x0000000017A00000
gJasonPlatformTokenSpaceGuid.PcdGicRedistributorBase|0x0000000017B00000
gJasonPlatformTokenSpaceGuid.PcdGicRedistributorLength|0x0000000000100000
```

ACPI 6.4 中 GICD 子表的 `GicVersion` 必须为 3。GICv3 CPU interface 通过 System Registers 访问，GICC 的 `PhysicalBaseAddress` 不再填写旧 GICv2 MMIO 地址。当前用一个 GICR 子表描述完整连续区域，GICC 的 `GICRBaseAddress` 留零；每核 MPIDR 直接来自 `/cpus`。EDK2 对应定义是 `EFI_ACPI_6_4_GICR_STRUCTURE`（16 字节）、`EFI_ACPI_6_4_GICR`（`0x0E`）和 `EFI_ACPI_6_4_GIC_V3`。

这些值修正了静态 ACPI 描述，但尚缺 `/proc/iomem`、真实 MADT 二进制解析及操作系统启动验证。

### 4.2 UFS

| 节点 | compatible | 寄存器资源 | 其他直接证据 |
|---|---|---|---|
| `/soc/ufshc@1da4000` | `qcom,ufshc` | `0x01DA4000 / 0x3000` | SPI `0x109`，`status = "ok"`，单 lane，non-removable |
| `/soc/ufsphy@1da7000` | `qcom,ufs-phy-qmp-v3-660` | `0x01DA7000 / 0xDB8` | `status = "ok"` |
| `/soc/ufsice@1db0000` | `qcom,ice` | `0x01DB0000 / 0x8000` | UFS ICE 时钟和总线引用 |

时钟 ID、reset、供电、总线带宽及 ufshcd 初始化顺序仍需对应内核驱动和真机日志。DTB 不含可直接报告给 Windows 的磁盘容量，UFS 驱动不得仅因寄存器地址已核实就启用。

### 4.3 SPMI、PMIC 与 ADC

`/soc/qcom,spmi@800f000`（`qcom,spmi-pmic-arb`）的五段资源为：

| reg-name | 基址 | 长度 |
|---|---:|---:|
| `core` | `0x0800F000` | `0x1000` |
| `chnls` | `0x08400000` | `0x1000000` |
| `obsrvr` | `0x09400000` | `0x1000000` |
| `intr` | `0x0A400000` | `0x220000` |
| `cnfg` | `0x0800A000` | `0x3000` |

子节点为 `qcom,pm660@0`、`qcom,pm660@1`、`qcom,pm660l@2`、`qcom,pm660l@3`。PM660@0 下还包括：

- `vadc@3100`：`qcom,qpnp-vadc-hc`，外设寄存器窗口 `0x3100 / 0x100`。
- `vadc@3400`：`qcom,qpnp-adc-tm-hc`，外设寄存器窗口 `0x3400 / 0x100`。
- `rradc@4500`：`qcom,rradc`，外设寄存器窗口 `0x4500 / 0x100`。

这些偏移必须结合 SPMI slave ID 和 PMIC 驱动语义使用，不能拼成未经验证的 flat MMIO。旧值 `0x01800000` 仍不作为 ADC 已确认地址。

### 4.4 UART、触摸、TLMM 与 USB

- `/soc/serial@0c170000`：`qcom,msm-uartdm-v1.4`，`0x0C170000 / 0x1000`，SPI `0x6C`，`status = "ok"`；`/chosen/stdout-path` 经 `serial0` 指向该节点。
- `/soc/i2c@c178000`：`qcom,i2c-msm-v2`，`0x0C178000 / 0x600`，QUP SPI `0x62`，`status = "okay"`。
- `synaptics_dsi_i2c@20`：`synaptics,dsx-i2c-force`，I2C 地址 `0x20`，TLMM IRQ GPIO 67、reset GPIO 66。供电、固件变体、复位时序和 pinctrl 仍是启用触摸驱动的必要条件。
- `/soc/pinctrl@03000000`：`qcom,sdm660-pinctrl`，`0x03000000 / 0xC00000`。
- `/soc/ssusb@a800000`：主 USB wrapper 基址 `0x0A800000`，子节点 `dwc3@a800000` 为 `snps,dwc3`，寄存器窗口 `0x0A800000 / 0xC8D0`，SPI `0x83`。
- `/soc/hsusb@c200000`：第二 USB wrapper 基址 `0x0C200000`，`status = "disabled"`；不得按已启用主控制器描述。

SSDT 和驱动仍需核对时钟、reset、PHY、供电及 pinctrl，不能仅复制 `reg` 后就操作硬件。

### 4.5 Wi-Fi/Bluetooth 与 PCIe 结论

`/soc/qcom,icnss@18800000` 的 `compatible = "qcom,icnss"`，第一段 `membase` 为 `0x18800000 / 0x800000`，并引用 SMMU 和多路供电。后两段 `reg` 的名称分别是 `smmu_iova_base`、`smmu_iova_ipa`，不能当作普通 CPU MMIO 窗口。根节点 `bt_wcn3990` 使用 `qca,wcn3990`，Slimbus 下也有 `wcn3990` 节点。

在这份完整 DTS 中未发现 PCIe host controller 节点或 PCIe compatible。故当前没有证据支持 QCA6174 的 PCIe BDF/BAR 描述，也不能把 ICNSS 的地址当成 PCIe BAR。Wi-Fi 驱动需要的固件加载、SMMU、远程处理器和电源流程仍未实现。

### 4.6 保留内存与 FDF 地址

固定 `reserved-memory` 节点明确包含：

| 区域 | 基址 | 长度 |
|---|---:|---:|
| `wlan_msa_guard` | `0x85600000` | `0x100000` |
| `wlan_msa_mem` | `0x85700000` | `0x100000` |
| `removed_regions` | `0x85800000` | `0x3700000` |
| `modem_fw_region` | `0x8AC00000` | `0x7E00000` |
| `adsp_fw_region` | `0x92A00000` | `0x2200000` |
| `pil_mba_region` | `0x94C00000` | `0x200000` |
| `cdsp_fw_region` | `0x94E00000` | `0x600000` |
| `splash_region` | `0x9D400000` | `0x23FF000` |
| `dfps_data_mem` | `0x9F7FF000` | `0x1000` |

另有 Venus、ADSP、QSEECOM、secure 和 CMA 等动态 `shared-dma-pool`，不能用固定节点列表代替完整运行时内存图。`Jason.fdf` 的 `BaseAddress = 0xA0000000` 只是历史容器地址；它未被上述固定区覆盖并不等于可安全加载，必须结合 bootloader 内存布局、`/proc/iomem`、TrustZone/远程处理器保留区和实际加载入口重新确认。

## 5. 真机交叉采集

以下命令需要可用的 adb；`/proc/iomem` 和完整设备树通常需要 root。无权限时保留错误信息，不要用猜测值补齐。

```bash
adb shell getprop ro.product.device
adb shell getprop ro.build.fingerprint
adb shell uname -a
adb shell 'su -c "cat /proc/iomem"' > iomem.txt
adb shell 'cat /proc/interrupts' > interrupts.txt
adb shell 'dmesg' > dmesg.txt
adb shell 'su -c "dtc -I fs -O dts /sys/firmware/devicetree/base"' > live-system.dts
adb shell 'ls -l /sys/firmware/devicetree/base' > device-tree-root.txt
```

重点比较运行中的 DT 与第 2 节镜像哈希对应的 DTS，并核对：

- GIC：`reg`、驱动日志、GIC 版本、CPU 在线拓扑。
- UFS：ufshcd 初始化、时钟/reset/PHY、实际容量和错误日志。
- 中断：DT SPI/PPI 编号与 ACPI GSI 的映射；`/proc/interrupts` 行号不能直接当 GSI。
- PMIC/ADC：SPMI slave、peripheral 地址和采样语义。
- 内存：所有固件、远程处理器、framebuffer 和 TrustZone 保留区。

## 6. 启动链、签名校验与 AVB

当前按通用 Qualcomm/Android 启动链记录，**jason 的逐阶段实际配置尚未验证**：

```text
PBL（芯片 ROM）
  → SBL/XBL（具体命名取决于该设备软件栈）
    → ABL/aboot（fastboot 与 Android 启动）
      → boot.img / recovery.img
        → Linux kernel + DTB + initramfs
```

通常前一阶段会验证后一阶段的 Qualcomm 签名或 Android 验证信息；ABL/aboot 还可能通过 AVB/vbmeta 验证 boot、vendor_boot、dtbo 等分区。bootloader unlock 状态、AVB 模式、dm-verity 和回滚索引必须分别确认，不能笼统视为“已绕过”。

只读检查示例：

```bash
fastboot getvar unlocked
fastboot getvar all
avbtool info_image --image boot.img
avbtool info_image --image vbmeta.img
```

当前状态：

- **尚未解决**：PBL→SBL/XBL→ABL/aboot 的实际校验算法、证书链和解锁限制。
- **尚未解决**：boot/vbmeta/dtbo 的 AVB footer、hash/hashtree、回滚索引与分区关系。
- **尚未解决**：UEFI 载荷的合法加载入口、内存保留、签名及返回 Android 的路径。
- `Scripts/InjectPayload.py` 仍是旧研究脚本，不得视为可用注入或刷写工具。

不要通过关闭校验、降级 bootloader 或写入未知签名来“测试”。任何写入实验都需要独立备份、可恢复启动路径和明确的人工确认。

## 7. 驱动实现状态

以下状态区分“代码已写”和“真机已验证”。当前没有任何一项经过 jason 实机启动验证。

### 7.1 UFS（内部储存识别）

`Drivers/StorageUfs` 现在是实际驱动，不再是占位：

- GCC 上电顺序：UFS GDSCR `0x75004`、BCR `0x75000`，以及 AXI/AHB/UNIPRO/ICE/PHY-AUX/CLKREF/TX-RX symbol 各分支 `0x75008`、`0x7500C`、`0x75010`、`0x75014`、`0x75034`、`0x76008`、`0x7600C`、`0x76040`、`0x88008`。这些偏移取自 `drivers/clk/qcom/gcc-sdm660.c`。
- HCI：按 UFS HCI 2.x 寄存器图初始化（`HCS 0x30`、`HCE 0x34`、`IS 0x20`、`UTRLBA 0x50`、`UICCMD 0x90`），发 `DME_LINKSTARTUP`。
- 读 Device Descriptor 与 Unit Descriptor（LUN 0），用 `bLogicalBlockSize` / `qLogicalBlockCount` 计算容量。
- 发布 `EFI_BLOCK_IO_PROTOCOL`，`READ(10)`/`WRITE(10)` 经 UTRD + PRDT 提交，DMA 缓冲 128 KiB 且带 cache 维护。

明确未实现，因此不能称为“已完成”：

- QMP UFS PHY serdes 校准表。代码只恢复 PHY 时钟与 UFS 复位，**依赖 bootloader 已把 PHY 上电**。若 PHY 已被关闭，链接会失败。
- HS gear 协商，链路停在 PWM-G1，读写慢。
- ICE 内联加密、RPMB、多 LUN、Boot LUN 切换。
- Windows 侧存储栈：UEFI 的 BlockIO 只让固件和 PE 看到磁盘；Windows 仍需要其自身的 UFS/存储驱动与 ACPI 绑定，本轮未提供 `_HID`。

失败时的行为：所有等待都有上限，失败只打日志并返回，不会挂死启动。可用 `PcdUfsEnabled` 关闭。

预期串口日志（成功路径）：

```text
Ufs: UFS version 0x... CAP 0x...
Ufs: UFS device: LUs=.. boot=.. spec=0x...
Ufs: LUN 0: <blocks> blocks of <n> bytes
Ufs: LUN 0 published as block device
```

### 7.2 USB 即插即用

`Drivers/UsbHost` 把 DWC3 xHCI 的 MMIO 窗口（`0x0A800000`，长度 `0xC100`）注册成 non-discoverable PCI 设备。DSC/FDF 已加入 `NonDiscoverablePciDeviceDxe`、`PciBusDxe`、`XhciDxe`、`UsbBusDxe`、`UsbMassStorageDxe`，以及 `DiskIoDxe`、`PartitionDxe`、`EnhancedFatDxe`。枚举成功后 UEFI Shell 可看到 `fs0:` 之类的 USB 存储卷，键鼠热插拔由 `UsbBusDxe` 处理。

未实现：USB PHY 与 GCC USB3 时钟重新初始化（依赖 bootloader 状态）；第二个控制器 `0x0C200000` 在 DTB 中是 `disabled`，保持不碰。

### 7.3 触控

`Drivers/TouchI2c` 已实现 TLMM GPIO（reset 66 / irq 67）与 QUP I2C tag 模式传输（QUP 偏移取自 `drivers/i2c/busses/i2c-qup.c`），复位后会做一次有界辨识读取并打印 ID。

**未发布 `EFI_ABSOLUTE_POINTER_PROTOCOL`**：S3330 的触摸报文布局随面板固件变化，DTB 里没有该信息，臆造解析只会产生一个不能用的指针设备。需要先在 Android 下抓一份真实报文，再补解析层。

### 7.4 其他

`Drivers/ClocksGpcd`、`Drivers/PmicRegulator`、`Drivers/WifiQca` 保持 `EFI_UNSUPPORTED`。其中 PMIC 已纠正为 **PM660/PM660L**（DTS 实际节点），此前写成的 PMI632 与设备不符。

## 8. 系统功能现状：电量 / 按键 / 音量 / 网络 / 相机

先说结论：**这些功能能否在 Windows 里出现，取决于 Windows 侧驱动，不取决于 UEFI 固件。** 固件能做的只有把硬件读出来、把资源描述清楚。逐项状态如下。

### 8.1 电量

- 硬件：PM660 的 FG gen3，DTB 节点 `qpnp,fg`，`compatible = "qcom,fg-gen3"`，SOC 块在 SPMI 从机 0 的外设 `0x4000`。
- 固件已实现：`Drivers/SpmI` + `Drivers/PmicBattery`。读 `0x4009` 两个字节，两字节一致才采信（对应 `qpnp-fg.c` 的 `get_monotonic_soc_raw()`），`0xFF` 视为 100%，结果通过 `gJasonBatteryProtocolGuid` 暴露并在启动日志打印 `Battery: NN%`。
- Windows 侧：仍未显示。Windows 的「Microsoft ACPI 兼容控制方法电池」要求 ACPI `PNP0C0A` 的 `_BIF`/`_BST` 由 AML 现场取值，而 AML 无法发起 SPMI 事务（SPMI 不是 MMIO，也不是 EC/SMBus，ACPI 没有对应 OperationRegion）。所以电量必须由 Qualcomm 的 PMIC 电池驱动在 Windows 侧直接读 SPMI，这个驱动没有公开的 jason/ARM64 版本。
- 未实现：电流、电压、温度、循环计数（在 FG SRAM，需先实现 FG memif）；充电状态（SMB2 charger 寄存器映射未核实）。

### 8.2 电源键与音量键

- 硬件（DTB `qcom,power-on@800`，PM660）：`pon_1` = `linux,code 0x74`（KEY_POWER），`pon_2` = `linux,code 0x72`（KEY_VOLUMEDOWN）；`RT_STS` 在 `0x0810`，`qpnp-power-on.c` 中 `QPNP_PON_KPDPWR_N_SET = BIT(0)`、`QPNP_PON_RESIN_N_SET = BIT(1)`。
- 音量加：PM660L GPIO 7，DTB `gpios = <&pm660l_gpios 7 1>`（1 = active low），外设 `0xC600`，`RT_STS` 在 `+0x10`，bit0 为 0 表示按下。
- 固件已实现：`Drivers/PmicKey` 读取以上三路并打印 `state power=.. vol_up=.. vol_down=..`，通过 `gJasonPmicKeyProtocolGuid` 暴露。
- Windows 侧：仍未响应。按键要变成 `VK_VOLUME_UP/DOWN`、电源菜单，需要 OS 侧驱动把 SPMI 事件转成输入/电源事件；ACPI 侧同样无法直接访问 SPMI。
- 硬件兜底：长按电源键的复位由 PON 的 S2 复位保证（DTB `qcom,support-reset` / `qcom,s2-timer`），与是否有 OS 驱动无关。

### 8.3 音量输出（扬声器/耳机）

- 硬件：Tasha codec（WCD9341），DTB 中为 `tasha_codec` / `compatible = "qcom,tasha-slim-pgd"`，挂在 SLIMbus（`slim-ngd@151c0000`，reg `0x151C0000 / 0x2C000` + `0x15184000 / 0x2A000`）上，另有 `msm-dig-codec` 与 PMIC 模拟 codec。同一份 DTB 里该 SLIMbus 实例是 `status = "disabled"`，真正启用的是哪一个实例还没有确认。
- 固件/Windows：都没有可用驱动。Windows 需要 SLIMbus 控制器 + codec 驱动 + audio class driver，公开渠道没有 ARM64 版本；ACPI 只能描述设备，不能替代驱动。所以「调节音量」目前只能到「按键可读」这一步，声音本身还出不来。

### 8.4 网络

- 硬件：WCN3990，经由 ICNSS（DTB `qcom,icnss@18800000`）而不是 PCIe；DTB 里没有 PCIe host 节点，之前的 QCA6174/PCIe 假设已作废。
- Windows 侧：需要 ICNSS 固件加载 + Windows WLAN 驱动，两者都没有公开的 jason 版本，所以 Wi-Fi 在 Windows 里显示不出来。
- 可行的替代路径（真实可用）：`Drivers/UsbHost` 已经带起 xHCI 栈，插 USB 网卡 / RNDIS / CDC-ECM 设备即可联网，Windows 自带这些 USB 网络类驱动，不需要额外移植。
- 已发布 ACPI 节点：`AcpiTables/ssdt_periph.asl` 里的 `WLAN`，`_HID = "JAS0001"`，`_CRS` 为 `0x18800000 / 0x800000`。这是 Jason 占位 ID，不是注册过的厂商 ID，作用是让硬件出现在固定的 ACPI 路径上并带上正确内存窗口；拿到真正的 Windows 10 ARM64 WLAN 驱动后，把 `_HID` 改成该驱动 INF 匹配的字符串即可，其他不用动。没有驱动时设备管理器里会是一个无驱动设备，即"识别到但不可用"。

### 8.5 相机

- 硬件：CAMSS + CCI（I2C）+ 摄像头 sensor，DTB 中 `qcom,cam-*` 节点齐全。
- Windows 侧：需要 sensor 驱动、CCI 控制器驱动和 AVStream 管线，公开渠道没有 jason 的对应驱动。固件最多只能把 CCI/I2C 与 GPIO 资源描述进 ACPI，设备管理器里会是个没有驱动的未知设备，没有实际意义，因此这次不发布假节点。

### 8.6 一句话汇总

| 功能 | UEFI 固件 | Windows 里可见 |
| --- | --- | --- |
| 电量百分比 | 已实现（SPMI + FG gen3） | 否，需 Qualcomm PMIC 电池驱动 |
| 电源键 / 音量键 | 已实现（PON + PM660L GPIO） | 否，需 OS 侧输入/电源驱动 |
| 音量输出 | 无 | 否，需 WCD9335 codec 驱动 |
| Wi-Fi | 无 | 否；USB 网卡可用 |
| 相机 | 无 | 否，需 sensor + AVStream 驱动 |
| 内部储存（UFS） | 已实现块设备 | 需 Windows UFS 驱动 + ACPI 绑定 |
| USB 即插即拔 | 已实现（xHCI 栈） | 是 |

### 8.7 Windows 10 ARM64 侧怎么接

本轮按 Windows 10 ARM64 为目标发布 ACPI 描述，`AcpiTables/ssdt_periph.asl` 里现成的节点：

- `XHC0`：`_HID = "PNP0D10"`，这是 ACPI 定义好的 xHCI 主机控制器 ID，Windows 10 ARM64 自带的 USB 栈会直接绑定，因此 **USB 设备在系统里是即插即用的**，不需要任何厂商驱动。内存窗口只描述 xHCI 寄存器块 `0x0A800000 / 0xC100`，DWC3 核心部分留给后续。
- `WLAN`：`_HID = "JAS0001"`（占位），资源 `0x18800000 / 0x800000`。
- `UFS0`：`_HID = "JAS0002"`（占位），UFS 主机、PHY、ICE 三段内存窗口。

接入真实驱动的步骤：

1. 拿到 Windows 10 ARM64 的 INF 后，看它 `[Manufacturer]`/硬件 ID 段里匹配的 ACPI HID，把对应节点的 `_HID` 换成该字符串，重新编译 ASL 即可，`_CRS` 不用改。
2. 驱动必须是 ARM64 架构并带签名；自签需要开测试签名模式，否则系统直接拒绝加载。
3. 若该驱动依赖 SMMU，需要额外提供 IORT；本平台暂时没有 IORT，ICNSS 在 DTB 里是 `qcom,smmu-s1-bypass`，说明硬件侧本来就旁路 SMMU，可以先按旁路处理。
4. 不要指望 Windows 使用固件侧的 `EFI_BLOCK_IO_PROTOCOL`：安装程序读盘走的是 Windows 自己的存储栈，固件的 BlockIO 只对 UEFI Shell 和 PE 阶段有意义。

## 9. 提交硬件证据时附带

1. 设备指纹、内核版本、镜像 SHA-256。
2. 原始 `iomem.txt`、`interrupts.txt`、`dmesg.txt` 和解压后的 DTS。
3. 每个修改值对应的 DTS 节点路径或内核源码文件/行号。
4. 哪些值已由 DTB 核实、哪些经过真机运行验证、哪些仍是假设；测试前后日志。
5. 不提交完整专有 ROM、签名私钥、账号令牌或个人数据。
