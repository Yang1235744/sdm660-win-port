# SDM660 WinPort — 快速启动指南（中文）

## 这是什么项目

给 **骁龙 660 芯片**（小米 Note3 / jason 代号）打造一套能让 Windows ARM64 跑起来的固件工程：
- UEFI/EDK2 引导层 → 从安卓启动链切入，接管后加载 `bootmgfw.efi`
- ACPI 表 → 告诉 Windows ARM64 硬件在哪、中断怎么路由
- 驱动骨架 → 显示、存储、USB、触摸、Wi-Fi、电源管理的参考实现

---

## 前置条件（开发机必须有）

```powershell
# ① WSL2 Ubuntu 或原生 Linux（Windows 本机无法直接编译 ARM64 UEFI）
# ② Python 3.9+
# ③ EDK II 源码
git clone https://github.com/tianocore/edk2.git
cd edk2 && git submodule update --init --recursive

# ④ ACPICA iASL 编译器（用于编译 .asl → .aml）
sudo apt install acpica-tools   # Debian/Ubuntu
```

---

## 构建步骤（在 WSL 中执行）

```bash
# 1. 克隆本仓库到 edk2 同级目录
git clone <your-repo-url>
cd sdm660-win-port

# 2. 配置 EDK2 环境
export WORKSPACE=$(pwd)/../edk2
export PACKAGES_PATH=$(pwd):$WORKSPACE
export EDK_TOOLS_PATH=$WORKSPACE/BaseTools

# 3. 初始化（只在第一次需要）
source edksetup.sh

# 4. 构建固件
python edksetup.py build \
    --platform JASON:XIAOMI_NOTE3:NULL \
    -a AARCH64 \
    -t GCC5 \
    -b RELEASE \
    -p UefiPlatform/UefiPlatform.dsc

# 输出产物位于 Build/JasonUefi/DEBUG_GCC5/FV/
```

单独编译 ACPI 表（不需要 EDK2）：
```bash
python3 AcpiTables/gcc_tables.py
```

---

## 核心文件导航

| 文件 | 作用 | 需要关注什么 |
|------|------|--------------|
| [README.md](README.md) | 项目总览 + 依赖说明 | 看"Build"章节 |
| [DEVELOPER.md](DEVELOPER.md) | 开发者手册（提交规范等） | 新功能前必读 |
| [UefiPlatform/Platform.h](UefiPlatform/Include/Hardware/Platform.h) | 所有硬件寄存器地址 | 需要核对 TRM/dtsi |
| [UefiPlatform/Platform.c](UefiPlatform/Platform.c) | UEFI DXE 入口主函数 | 启动顺序在这里编排 |
| [UefiPlatform/UefiPlatform.dsc](UefiPlatform/UefiPlatform.dsc) | EDK2 DSC 描述 | 增减驱动改这里 |
| [Drivers/\*](Drivers/) | 各功能驱动 | 先验 ClocksGpcd 再 StorageUfs |
| [AcpiTables/ssdt_cpu.asl](AcpiTables/ssdt_cpu.asl) | CPU 拓扑 ACPI 表 | `_UID`/GSI 必须实测对 |
| [AcpiTables/ssdt_periph.asl](AcpiTables/ssdt_periph.asl) | UART/I2C/GPIO/WDT ACPI 表 | IRQ 号来自 dmesg |
| [Docs/arch.md](Docs/arch.md) | 整体架构图 + 内存映射 | 理解启动流程必读 |
| [Docs/roadmap.md](Docs/roadmap.md) | 里程碑清单 | 对照进度追踪 |

---

## 当前状态与已知缺口

✅ 已完成（本次生成的）
- 项目骨架 + 可解析的 C 代码 + ASL 表
- Platform.h 全部 MMIO 基地址（标注来源为 sdm660.dtsi）
- 时钟树、存储、USB、I2C 触摸、PMIC、WiFi（stub）6 个驱动模块
- 完整的 EDK2 平台描述（DSC/INF/DEC）和 Makefile

⚠️ 待验证 / 待填坑
1. GPLL 锁定时长 — 实际硅片可能需要 5ms 以上，当前 10ms 重试窗口
2. UFS 容量 — placeholder `0x00800000` 扇区数，需要从真实设备读取
3. PCIe BAR 地址 — WiFi QCA6174 的实际 slot/function 未知
4. MADT GICC entry — 需要拿 `dmesg | grep gsi` 实测对一遍 IRQ
5. DTB 注入路径 — `Scripts/InjectPayload.py` 尚未编写（Phase 1 目标）

---

## 下一步建议（按优先级）

1. **拿到一台刷过 LineageOS 的 jason**，执行以下命令收集数据：
   ```bash
   dmesg \| grep -E "memblock|gsi|gpll|qup|i2c|ufshc|blsp" > hardware-baseline.txt
   cat /sys/firmware/dtb/* 2>/dev/null || dtc -I fs /sys/firmware/devicetree/base > dtb.dts
   ```
2. 把 `hardware-baseline.txt` 贴进 PR，我们就能把占位符值换成真值。
3. 同步把 `Scripts/InjectPayload.py` 写完——这是把 UEFI 塞进 aboot 的关键一环。
