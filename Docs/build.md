# WSL / Linux 构建说明

## 1. 产物边界

本工程使用标准 EDK2 DSC/DEC/INF/FDF 构建 **jason 研究用 DXE 模块和固件卷容器**。当前没有完整 SEC、PEI、DXE Core、Boot Manager、签名链及可恢复刷写流程，因此生成的 `.fd` 不能直接刷入手机分区，也不代表能够加载 Windows。

## 2. 环境

推荐 WSL2 Ubuntu 或原生 Ubuntu。初始验证基线建议使用 `edk2-stable202411`；更换 EDK2 版本后需重新核对库实例和 ACPI 头文件。

```bash
sudo apt update
sudo apt install build-essential git make python3 python3-distutils uuid-dev \
  acpica-tools gcc-aarch64-linux-gnu
```

DTB 提取只需 Python 3 标准库，不参与 EDK2 构建；可选 `pyfdt` 仅用于生成 DTS。

## 3. 源码布局

将两个仓库放在同一父目录：

```text
work/
├── edk2/
└── sdm660-win-port/
```

获取指定版本：

```bash
git clone --branch edk2-stable202411 --depth 1 https://github.com/tianocore/edk2.git
git clone https://github.com/Yang1235744/sdm660-win-port.git
cd edk2
git submodule update --init --recursive
make -C BaseTools -j"$(nproc)"
```

## 4. 配置并构建

以下命令从 `edk2` 目录执行：

```bash
export WORKSPACE=$PWD
export PACKAGES_PATH=$WORKSPACE:$WORKSPACE/../sdm660-win-port
export EDK_TOOLS_PATH=$WORKSPACE/BaseTools
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
source edksetup.sh

build -a AARCH64 -b RELEASE -t GCC5 -p UefiPlatform/UefiPlatform.dsc
```

关键点：

- WSL/Linux 使用 `source edksetup.sh` 和 `build`，不是 `python edksetup.py build`。
- `-p` 必须完整写为 `UefiPlatform/UefiPlatform.dsc`。
- `PACKAGES_PATH` 同时包含 EDK2 与本仓库，使 `MdePkg/...` 和 `UefiPlatform/...` 均可解析。
- 交叉编译器前缀以实际安装工具为准，可用 `aarch64-linux-gnu-gcc --version` 检查。
- DEBUG 构建将 `-b RELEASE` 改为 `-b DEBUG`，输出目录也相应改变。

## 5. 预期路径

RELEASE/GCC5 的主要输出目录：

```text
$WORKSPACE/Build/JasonUefi/RELEASE_GCC5/
├── AARCH64/                 # 各模块的中间文件和 .efi
└── FV/                      # FV、FD 及 FDF 生成产物
```

重点检查 `FV/` 下的 `JASON_PAYLOAD.fd`、`JASON_MAIN_FV` 相关文件以及各模块 `.efi`。具体中间文件名由所用 EDK2/BaseTools 版本决定。

`Jason.fdf` 暂定 FD 映射基址 `0xA0000000`、大小 `0x00400000`，仅用于形成研究容器；该地址尚未依据 jason 内存保留图验证，不能据此加载或刷写。

## 6. GIC 配置与运行时行为

DSC 中 GICD/GICR PCD 已按匹配 jason boot.img 的 `interrupt-controller@17a00000` 填写：GICD `0x17A00000`，GICR `0x17B00000`，GICR 长度 `0x100000`。节点明确为 `arm,gic-v3`，旧 GICv2 GICC MMIO PCD 已删除。若基址为零或 GICR 长度不足，`MadtGenerate()` 失败，`AcpiTablesEntryPoint()` 返回 `EFI_NOT_READY`，不会安装 MADT。

GICC 使用 ACPI 6.4 标准 80 字节结构，头部 `Reserved` 为 **2 字节**，`MPIDR` 为 **64 位**；GICD 子表为 Type `0x0C`、24 字节，GICR 为 Type `0x0E`、16 字节，GICC 为 Type `0x0B`，MADT Revision 为 `0x05`。GICD 的 `GicVersion = 3`；GICv3 GICC 的 `PhysicalBaseAddress` 为零。当前修正尚未经过真机 MADT 解析或操作系统启动验证。

## 7. 单独构建 ACPI SSDT

```bash
cd ../sdm660-win-port
python3 AcpiTables/gcc_tables.py --output Build/acpi
# 或
make acpi
```

只编译 `ssdt_cpu.asl` 和 `ssdt_periph.asl`。`rsdp.asl` 只是历史兼容说明文件：RSDP/XSDT 由 EDK2 ACPI 框架生成，MADT 由 C 生成器创建。

当前 `AcpiTables.c` 只安装运行时 MADT。FDF 中的 SSDT RAW 打包不等同于运行时安装；完整 SSDT 安装路径仍需后续实现和验证。

## 8. 常见错误

- **找不到 UefiPlatform.dec**：检查 `PACKAGES_PATH` 是否包含本仓库根目录，路径分隔使用 Linux `:`。
- **找不到 BaseLib**：使用 `MdePkg/Library/BaseLib/BaseLib.inf`，不要引用不存在的 ArmPkg BaseLib 路径。
- **找不到 aarch64 编译器**：检查 `GCC5_AARCH64_PREFIX` 和 `PATH`。
- **BaseTools 命令缺失**：先 `make -C BaseTools`，再重新 `source edksetup.sh`。
- **iASL 没有生成 AML**：脚本使用 `iasl -p <输出前缀> <源文件>`，不是生成 C 表的 `-tc`。
- **ACPI 驱动返回未就绪**：先检查两个 GIC PCD；默认拒绝安装是预期行为。

构建日志、静态检查和真机验证是三件不同的事。提交时分别说明，不要把“编译通过”描述为“手机可启动”。
