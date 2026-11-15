# 贡献指南

## 基本原则

1. **硬件值必须有证据**：寄存器地址、MPIDR、GSI、compatible、BDF、时钟和分区布局需附 jason DTB/DTS、`/proc/iomem`、内核源码或 TRM 来源。
2. **区分状态**：明确标注“静态修改 / 构建通过 / AML 编译通过 / 真机验证”，不得把其中一项当成另一项。
3. **默认安全**：未验证的驱动不得因 DXE 加载就修改时钟、PMIC、存储、USB 或分区状态。
4. **不提交危险产物**：不要提交私钥、账号令牌、完整专有 ROM、签名固件或未说明用途的可刷写二进制。

## 开发流程

1. 从小范围修改开始，保持相关文件和文档同步。
2. 在 WSL/Linux 按 `Docs/build.md` 执行 EDK2 构建，保存完整错误日志。
3. 修改 ASL 时运行：

   ```bash
   python3 AcpiTables/gcc_tables.py --output Build/acpi
   ```

4. 修改 Python 工具时至少执行语法检查和 `--help`；没有匹配的 jason 镜像时，不得声称已提取真实 DTS。
5. 真机实验前保留原始镜像和哈希，记录设备指纹、内核版本、采集文件及恢复路径。不要在 PR 中鼓励他人直接刷写未验证产物。

## C / EDK2

- 使用 EDK2 当前 API：MMIO 采用 `MmioRead32/MmioWrite32`，不要恢复已移除的 `READ_REGISTER_UINT32` 等宏。
- INF 的 `ENTRY_POINT` 必须在 Sources 中真实存在；包路径、LibraryClasses 和 Includes 与实际使用一致。
- PCD 类型和 Token 在 DEC 声明，DSC 只写 `PCD|值`；FV/FD 布局只放 FDF。
- MADT 使用 `IndustryStandard/Acpi64.h` 标准结构，不自定义近似布局。
- 不用 RAM 窗口、其他 SoC 或其他机型的地址推导 jason 外设地址。

## ACPI

- RSDP/XSDT 由 EDK2 ACPI 框架生成，MADT 是独立顶层表，不能伪装成 SSDT Device。
- GIC 地址未配置时必须继续拒绝安装 MADT。
- SSDT 设备的 `_HID`、`_CID`、`_CRS` 和 Interrupt 资源只有在获得对应 Windows 绑定及硬件证据后才能添加。
- iASL 命令使用 `iasl -p <输出前缀> <源文件>`；任何一张源表失败都应使脚本失败。

## 提交说明与评审

提交说明应列出：修改目的、数据来源、执行过的命令、未执行的项目及剩余风险。硬件相关 PR 优先附脱敏后的 DTS 片段、`iomem` 行和内核日志，而不是只贴寄存器常量。

当前阶段尤其欢迎：原始 DTB/DTS 采集、EDK2 构建错误修复、ACPI 规范校验和可复现的只读硬件调查。
