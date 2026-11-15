# SDM660 WinPort 快速指南

## 先确认当前边界

本工程是 **jason（小米 Note 3 / SDM660）Windows ARM64 研究骨架**。当前 FD/FV 只包含平台、ACPI 和驱动研究模块，不是完整 UEFI 启动固件，不能直接刷入 `boot`/`aboot` 分区，也尚未验证 `bootmgfw.efi` 或 Windows 启动。

## 1. 准备 WSL2 / Linux

```bash
sudo apt update
sudo apt install build-essential git python3 make acpica-tools gcc-aarch64-linux-gnu
```

从 `boot.img` 提取匹配 DTB 只需 Python 3 标准库，不需要 `unpackbootimg` 或 `dtc`。如需同时生成 DTS，可额外安装 `python3 -m pip install pyfdt`；具体用法见 `Docs/hardware-notes.md`。

## 2. 获取源码并构建

将 EDK2 和本仓库放在同一父目录：

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

预期输出目录：`$WORKSPACE/Build/JasonUefi/RELEASE_GCC5/FV/`。详细排错见 [Docs/build.md](Docs/build.md)。

仅编译 SSDT：

```bash
cd ../sdm660-win-port
python3 AcpiTables/gcc_tables.py --output Build/acpi
```

## 3. 先采集真机数据

```bash
python3 Scripts/ExtractBootDtb.py --boot-img /path/to/jason-boot.img --output hardware/jason-boot
```

在有 root 权限的 jason Android shell 中采集：

```bash
cat /proc/iomem > iomem.txt
cat /proc/interrupts > interrupts.txt
dtc -I fs -O dts -o live-system.dts /sys/firmware/devicetree/base
dmesg > dmesg.txt
```

不要在未核对 DTS、`iomem` 和内核驱动前填入 GIC/UFS/PCIe/PMIC 地址。完整项目见 [Docs/hardware-notes.md](Docs/hardware-notes.md)。

## 4. 代码入口

| 文件 | 作用 |
|---|---|
| [UefiPlatform.dsc](UefiPlatform/UefiPlatform.dsc) | 平台库、组件和 GIC PCD 配置 |
| [Jason.fdf](UefiPlatform/Jason.fdf) | FD/FV 布局，不是 DSC 段 |
| [madt_gen.c](AcpiTables/madt_gen.c) | 标准 ACPI 6.4 MADT/GICC/GICD 生成 |
| [AcpiTables.c](AcpiTables/AcpiTables.c) | 通过 `EFI_ACPI_TABLE_PROTOCOL` 安装 MADT |
| [Platform.h](UefiPlatform/Include/Hardware/Platform.h) | 未验证寄存器研究值及警告 |
| [hardware-notes.md](Docs/hardware-notes.md) | 真机证据与签名链状态 |

当前所有硬件驱动入口均默认返回 `EFI_UNSUPPORTED`；只有完成数据核对和独立实现评审后才应启用硬件访问。
