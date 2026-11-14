# SDM660 WinPort — Git 初始化 & 首次提交（给开发者看）

本机未安装 git，以下命令请在有 git 的终端（WSL / PowerShell / cmd）中运行：

```powershell
cd "J:\文档\Trae-CN\temp_20261114_02"

# Step 1 — 初始化仓库（如果还没初始化）
git init --initial-branch=main
git config core.longpaths true
git config user.name    "sdm660-win-port"
git config user.email   "sdm660@github.local"

# Step 2 — 添加全部文件（排除 .gitignore 中列出的）
git add -A
git status        # 确认无遗漏或误加的文件

# Step 3 — 首次提交
git commit -m "$(cat <<'EOF'
Initial commit: SDM660 UEFI firmware skeleton + Windows ARM64 ACPI tables

Project scaffolding for porting Windows ARM64 onto Qualcomm SDM660 SoC
(Xiaomi Mi Note 3, codename jason).

Key deliverables:
- EDK II platform descriptor (DSC/INF/DEC) with 6 driver modules
- ACPI table sources (ASL): CPU topology, GICC MADT, peripheral mapping
- ARM64 driver skeletons: ClocksGpcd, StorageUfs, UsbHost, TouchI2c, PmicRegulator, WifiQca
- InjectPayload.py: tool to patch aboot/boot.img for UEFI injection
- madt_gen.c: runtime MADT generator with checksum and 8-core GICC entries
- Full hardware register map (Platform.h) annotated with sdm660.dtsi sources
- Apache-2.0 license; docs in Chinese and English

Next milestones tracked in Docs/roadmap.md
EOF
)"

# Step 4 — 推送到 GitHub（需要账号）
git remote add origin https://github.com/<your-user>/sdm660-win-port.git
git push -u origin main
```

## 验证清单

- [ ] `git log --oneline` 显示一个包含上述 commit message 的记录
- [ ] GitHub 仓库页面可见所有 *.c、*.asl、*.dsc、*.inf 源文件
- [ ] README.md 在仓库首页正确渲染（含 badges 可选）
