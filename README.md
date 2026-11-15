# SDM660 Windows ARM64 Port（jason）

面向 **Qualcomm Snapdragon 660 / Xiaomi Mi Note 3（jason）** 的开源研究工程，目标是逐步验证 Android 启动链、UEFI DXE 载荷、ACPI 硬件描述以及 Windows ARM64 启动所需驱动。

> **当前状态：研究骨架，不能刷机，也不能启动 Windows。** 仓库尚未包含完整的 SEC/PEI/DXE Core、经过验证的 boot.img 注入链或可用 Windows 驱动。错误的固件、分区或 MMIO 操作可能永久损坏设备。

许可证：[Apache-2.0](LICENSE)。

## 当前内容

| 目录 | 内容 | 状态 |
|---|---|---|
| `UefiPlatform/` | DSC、DEC、FDF 与安全平台入口 | 可进入 EDK2 构建流程；硬件初始化默认禁用 |
| `AcpiTables/` | ACPI 6.4 MADT 生成器、CPU SSDT、外设 SSDT 模板 | 按匹配 DTB 生成 GICv3 MADT；尚未真机启动验证 |
| `Drivers/` | 时钟、UFS、USB、触摸、SPMI/PMIC、Wi-Fi 研究代码 | UFS 驱动已实现块设备；USB 走 xHCI 栈；SPMI 主控 + 按键 + 电量可读；触控仅总线和辨识；其余入口返回 `EFI_UNSUPPORTED` |
| `Scripts/` | 纯 Python QCDT/DTB 提取及旧版注入研究脚本 | 不提供可刷写验证结果 |
| `Docs/` | 构建、硬件采集、签名链和架构说明 | 持续补充真机证据 |

## 构建环境

推荐使用 **WSL2 Ubuntu 或原生 Linux**，并安装：

- Python 3、`git`、`make`、`build-essential`
- `aarch64-linux-gnu-gcc` 交叉工具链
- ACPICA `iasl`
- EDK2 源码及其子模块（建议固定到经过验证的 stable tag）

Ubuntu 依赖示例：

```bash
sudo apt update
sudo apt install build-essential git python3 make acpica-tools gcc-aarch64-linux-gnu
```

## 构建

将 `edk2` 与本仓库放在同一父目录：

```bash
git clone https://github.com/tianocore/edk2.git
git clone https://github.com/Yang1235744/sdm660-win-port.git

cd edk2
git submodule update --init --recursive
make -C BaseTools

export WORKSPACE=$PWD
export PACKAGES_PATH=$PWD:$PWD/../sdm660-win-port
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
source edksetup.sh

build -a AARCH64 -b RELEASE -t GCC5 -p UefiPlatform/UefiPlatform.dsc
```

产物位于 `$WORKSPACE/Build/JasonUefi/RELEASE_GCC5/`，FD/FV 文件在其中的 `FV/` 目录。`JASON_PAYLOAD.fd` 仅是本工程模块的固件卷容器，**不是完整可刷写固件**。

单独编译 SSDT：

```bash
python3 AcpiTables/gcc_tables.py --output Build/acpi
```

RSDP/XSDT 由 EDK2 ACPI 框架生成，不能将 `rsdp.asl` 当作普通 SSDT 安装。完整说明见 [构建文档](Docs/build.md)。

## 已从 boot.img 核实的 GICv3 配置

上传镜像中唯一匹配 jason 根 `model` 的 DTB 明确描述 `arm,gic-v3`。`UefiPlatform.dsc` 现按该节点配置：

```ini
gJasonPlatformTokenSpaceGuid.PcdGicDistributorBase|0x0000000017A00000
gJasonPlatformTokenSpaceGuid.PcdGicRedistributorBase|0x0000000017B00000
gJasonPlatformTokenSpaceGuid.PcdGicRedistributorLength|0x0000000000100000
```

MADT 生成器使用 GICD、GICR 和 8 个 GICC 子表；GICv3 不再使用旧 GICv2 `PcdGicCpuInterfaceBase`。8 核 MPIDR 与 `/cpus` 一致。证据哈希、节点路径和未验证项见 [硬件说明](Docs/hardware-notes.md)。这些修改只修正静态 ACPI 描述，不代表 Windows 启动或真机 GIC 初始化已验证。

## 真机数据缺口

1. **[OPEN]** PLATCTL 基址及解锁位偏移；不要与 GPLL 锁定位混用。
2. **[DTB 已核实 / 运行待验证]** GICD/GICR、GICv3 和各核 MPIDR；仍需真机数据及 MADT 启动验证。
3. **[DTB 已核实 / 驱动禁用]** UFS HC/PHY/ICE 寄存器窗口；时钟、复位、初始化顺序和实际容量仍待验证。
4. **[结论修正]** Wi-Fi/Bluetooth 证据指向 WCN3990/ICNSS，未发现 PCIe host；不得假设 QCA6174 BDF/BAR。
5. **[DTB 已核实 / 驱动禁用]** SPMI、PM660/PM660L、VADC/ADC-TM/RRADC 节点；采样与电源语义仍需对应驱动和真机日志。
6. **[DTB 已核实 / 驱动禁用]** UART、QUP、TLMM、USB 和触摸资源；完整时钟、复位、PHY、供电及 pinctrl 流程仍未实现。
7. **[OPEN]** UEFI 安全装载地址和完整保留内存图；FDF 的 `0xA0000000` 不能视为已验证。

先按 [硬件说明](Docs/hardware-notes.md) 交叉采集数据，再考虑启用对应驱动。[ExtractBootDtb.py](Scripts/ExtractBootDtb.py) 只用 Python 标准库即可提取匹配 DTB；安装可选 `pyfdt` 后同时生成 DTS。

## 目录

```text
UefiPlatform/             EDK2 平台 DSC/DEC/FDF 与入口
AcpiTables/               MADT 生成器、SSDT 和 iASL 脚本
Drivers/                   九个 ARM64 驱动研究模块
Scripts/ExtractBootDtb.py boot.img → DTB → DTS 安全提取工具
Docs/build.md             WSL/EDK2 构建说明
Docs/hardware-notes.md    真机数据采集、AVB 与签名链状态
CONTRIBUTING.md           贡献与验证要求
```

## 贡献

请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。寄存器、IRQ、compatible、MPIDR 和分区布局的修改必须附带来源；真机测试需附采集日志。不要提交凭据、私有固件、完整受版权保护的 ROM 或未经说明的可刷写二进制。
