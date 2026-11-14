# 贡献指南 — SDM660 WinPort

## 一、分支模型

```
main            ← 稳定可用版本，永远可构建
develop         ← 日常开发主线，可能包含半成品
feature/<xxx>   ← 从 develop 拉出的单个功能分支（如 feature/acpi-madt）
```

提交前务必 `git checkout feature/xxx`，PR 目标为 `develop`；只有里程碑完成后再合入 `main`。

---

## 二、代码规范（C）

- **编译器**: clang (AArch64 target, `-march=armv8-a -mtune=cortex-a73`)
- **警告开关**: `-Wall -Wextra -Werror=return-type` 全部开启
- **命名**:
  - 函数: PascalCase，动词开头 → `EnableClock`, `UfsReadSectors`
  - 宏/常量: UPPERCASE_WITH_UNDERSCORE → `GPLL_LOCK_STATUS_BIT`
  - 结构体: CamelCase → `UfsDeviceInfo`
- **注释语言**: 中英皆可，硬件偏移量注释**必须**附带数据源（TRM 章节号或 dtsi 节点名）
- **禁止事项**:
  - ❌ 任何硬编码的魔法数值（必须定义为宏并写明来源）
  - ❌ 裸 `while(1)` 死循环（必须有退出条件或 timeout）
  - ❌ 跨模块未声明的全局变量

---

## 三、ACPI 表编写规范

| 规则                                                         |
|--------------------------------------------------------------|
| 每个 SSDT 文件名与 Device Path 中 `_ADR` 唯一对应               |
| IRQ 值必须来自 `dmesg \| grep gsi` 实测结果                    |
| 内存资源使用 `Memory32Fixed` / `Memory64Fixed`，不用 `IOPort`   |
| `.asl` 文件顶部添加当前维护者联系信息注释                       |
| 编译后检查 `.aml` size < 4 KB，超限需拆分                      |

---

## 四、提 PR 前自检清单

- [ ] `make check` 通过（无 compile error）
- [ ] `make acpi` 产出预期 `.aml` 文件
- [ ] 新寄存器地址已在 `Platform.h` 注释中标注 dtsi 节点名称
- [ ] 如涉及真实设备测试，已提供 `dmesg` 片段作为证据
- [ ] 更新 `Docs/roadmap.md` 对应阶段的 `[x]` 标记

---

## 五、硬件调试常用命令（在 jason 安卓 shell 中执行）

```bash
# 列出所有 memory region（验证 Base Address）
dmesg | grep -E "memblock|reserved"

# UART / BLSP clock base
dmesg | grep uart

# QUP/I2C/SPI controller addresses
cat /proc/iomem | grep -i qup

# GIC distributor base (用于 MADT)
cat /proc/interrupts | head -5
dmesg | grep -i gic

# UFS/eMMC geometry（用于容量校验）
cat /sys/block/mmcblk0/size
```

把上面的输出贴进 PR，可以大幅加速审核流程。
