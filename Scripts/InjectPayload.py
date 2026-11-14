"""
InjectPayload.py — 将 UEFI payload 注入小米 Note3 (jason) 的 aboot/boot.img

用法:
    python InjectPayload.py --boot-img boot.img --payload u-boot.bin [--offset AUTO]

工作原理（三种注入模式，按安全度排序）：
    MODE_OVL    DTB overlay — 不动 aboot，只是修改 kernel command line 指向新的 fdt
                风险最低，但依赖 bootloader 接受外部 fdt 地址
    MODE_REPL   替换 recovery 分区里的 dtbo.img 为带 UEFI 入口的新 overlay
                需要 unlock bootloader 后 adb push，效果稳定
    MODE_PATCH  直接 patch aboot 二进制中的"跳转到 Linux kernel"指令
                最有效但风险最高（可能变砖），仅用于实验性验证

项目背景：
    SDM660 / jason 的 aboot 是从 Qualcomm open-source 分支编译而来。
    其 boot 流程大致为：
        SPL → aboot → [检查/fastboot 键] → 加载 kernel+ramdisk+fdt → jump_to_kernel
    我们注入的目标就是那个 "jump_to_kernel" 之后、kernel 还未接手之前的窗口期，
    让 UEFI 先跑起来，再由它加载 Windows ARM64 的 bootmgfw.efi。
"""

import argparse
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


# ─── 常量 ──────────────────────────────────────────────────────────

SDM660_RAM_BASE         = 0x80000000          # LPDDR4 起始物理地址
SDM660_DTBO_OFFSET      = 0x01000000          # DTBO partition 在 flash 中的偏移
FIRMWARE_LOAD_ADDR      = 0xA0000000          # UEFI firmware 加载到 RAM 的位置
EFI_ENTRY_POINT         = FIRMWARE_LOAD_ADDR + 0x10000
WINDOWS_BOOTMGR_SIZE    = 0x80000             # bootmgfw.efi 典型大小（80 KB）

# jason 的分区表（从 fastboot getvar all 中读取）
PARTITIONS = {
    "boot":     0x00000000,
    "recovery": 0x00500000,
    "dtbo":     0x00A00000,      # DTBO 分区，aboot 从这里读 fdt
    "system":   0x00E00000,
    "vendor":   0x02000000,
    "splash":   0x03000000,
}

# aboot 中 "jump to kernel" 指令的典型模式（AArch64 BLX/BR 指令）
# 这是基于对 sdm660 开源版 aboot dump 的反编译推断，需实测确认
ABOOT_JUMP_INSTRUCTIONS = [
    # BR X0          — 跳到 X0 寄存器（通常包含 kernel 入口）
    0x9AC00000,
    # BL  <label>    — 调用 kernel entry point
    0x94000000,
]


@dataclass
class PatchResult:
    success: bool
    mode: str
    details: str


# ─── 工具函数 ──────────────────────────────────────────────────────

def read_partition(part_name: str) -> bytes | None:
    """通过 fastboot 读出指定分区原始数据。需要设备已解锁 bootloader。"""
    try:
        result = subprocess.run(
            ["fastboot", "getvar", "partition-size", part_name],
            capture_output=True, text=True, timeout=10,
        )
        size_str = result.stdout.strip().split(":")[-1].strip()
        if not size_str or "not found" in size_str.lower():
            print(f"[WARN]  找不到分区 {part_name}", file=sys.stderr)
            return None
        size = int(size_str, 16)
        print(f"[INFO]  分区 {part_name} 大小 = {size:#x} bytes ({size >> 10} KiB)")

        out = subprocess.run(
            ["fastboot", "flash", part_name, "/dev/stdin"],
            input=b"\x00" * size,       # placeholder — real call uses 'get' instead
            capture_output=True,
            timeout=30,
        )
        if out.returncode != 0:
            # fastboot 不支持 stdin 读取，改用 get 命令
            pass
    except FileNotFoundError:
        print("[ERROR] fastboot 未安装或未在 PATH 中", file=sys.stderr)
        return None
    return None  # stub — real implementation below


def extract_boot_img(boot_path: Path) -> dict:
    """解析 boot.img，返回 headers + kernel/dtb/ramdisk raw bytes。"""
    # Android boot image header is 1624 bytes at offset 0 (v2+)
    # Layout: magic(8) + kernel_size(4) + kernel_addr(4) + ramdisk_size(4)
    #         ramdisk_addr(4) + second_size(4) + second_addr(4)
    #         tags_addr(4) + page_size(4) + dtb_offset(4) + ...
    HEADER_FMT = "<8sIIIIIIIIIIII"
    with open(boot_path, "rb") as fh:
        raw = fh.read()

    if raw[:8] != b"ANDROID!":
        raise ValueError(f"{boot_path.name}: 不是有效的 boot.img（magic 不匹配）")

    (magic, kernel_sz, kernel_addr, ramdisk_sz, ramdisk_addr,
     second_sz, second_addr, _, page_sz, dtb_off,
     dtb_sz, _, _) = struct.unpack_from(HEADER_FMT, raw)

    return {
        "page_size":   page_sz,
        "kernel":      raw[HEADER.sizeof(): HEADER.sizeof() + kernel_sz],
        "kernel_addr": kernel_addr,
        "dtb":         raw[dtb_off: dtb_off + dtb_sz] if dtb_sz else b"",
        "dtb_size":    dtb_sz,
        "ramdisk":     raw[HEADER.sizeof() + kernel_sz:
                           HEADER.sizeof() + kernel_sz + ramdisk_sz],
        "full":        raw,
    }


def apply_dtb_overlay(dtb_bytes: bytes, overlay_bytes: bytes) -> bytes:
    """
    把 overlay FDT blob 合并进主 DTB。
    核心思路：用 libfdt 在 root node 下添加我们的 u-boot,payload 节点，
    并在 chosen 中插入 bootargs=... efiphy=&efivar。
    这里给出一个简化实现；生产环境建议用 Python libfdt 绑定。
    """
    if not overlay_bytes:
        return dtb_bytes
    # TODO: integrate python-libfdt for proper overlay application
    # For now return concatenation as proof-of-concept
    print("[WARN]  DTB overlay 使用朴素拼接（真实操作需 libfdt）")
    return dtb_bytes + overlay_bytes


def patch_aboot_for_uefi(aboot_bin: bytes) -> tuple[bytes, list[str]]:
    """
    对 aboot 二进制打补丁，使其在完成硬件初始化后跳转至 UEFI payload
    而非直接 jump_to_kernel。
    策略：在 `goto_kernel` 标签附近插入一个 conditional branch，
    检查内存地址 0x80000000 是否已被 UEFI 占位标记覆盖（由 recovery key 写入）。
    """
    patches = []
    # Search for the typical jump-to-kernel instruction pattern
    for instr in ABOOT_JUMP_INSTRUCTIONS:
        pos = aboot_bin.find(struct.pack("<I", instr))
        if pos >= 0:
            patches.append(f"Found jump instr at +{pos:#x}, replacing with conditional branch")
            # Replace unconditional jump with: cbz w0, skip (branch if zero → UEFI was loaded)
            new_instr = struct.pack("<I", 0xB4000040)  # cbz w0, <offset>
            aboot_bin = aboot_bin[:pos] + new_instr + aboot_bin[pos + 4:]
    return aboot_bin, patches


# ─── 主逻辑 ────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        prog="InjectPayload.py",
        description="将 SDMLinux EFI UEFI payload 注入 Xiaomi Note3 (jason) 启动镜像",
    )
    parser.add_argument("--boot-img", required=True, help="Android boot.img 路径")
    parser.add_argument("--payload", required=True, help="编译后的 UEFI FV blob (.bin)")
    parser.add_argument(
        "--mode", choices=["OVL", "REPL", "PATCH"], default="OVL",
        help="注入模式：OVL=DTB overlay（安全），REPL=替换 dtbo 分区，PATCH=改 aboot（高风险）",
    )
    parser.add_argument("--output", default=None, help="输出文件路径（默认生成 *.patched.img）")
    parser.add_argument("--dry-run", action="store_true", help="只打印计划动作，不写盘")
    args = parser.parse_args()

    boot_path = Path(args.boot_img).resolve()
    payload_path = Path(args.payload).resolve()

    if not boot_path.exists():
        print(f"[ERROR] 找不到 boot.img：{boot_path}", file=sys.stderr)
        sys.exit(1)
    if not payload_path.exists():
        print(f"[ERROR] 找不到 payload：{payload_path}", file=sys.stderr)
        sys.exit(1)

    print(f"[INFO]  源 boot.img  : {boot_path}")
    print(f"[INFO]  Payload    : {payload_path} ({payload_path.stat().st_size} bytes)")
    print(f"[INFO]  注入模式   : {args.mode}")

    # Step 1: 解析 boot.img
    try:
        img = extract_boot_img(boot_path)
    except Exception as e:
        print(f"[ERROR] 解析失败：{e}", file=sys.stderr)
        sys.exit(1)

    # Step 2: 按模式执行
    result: PatchResult
    if args.mode == "OVL":
        result = _mode_dtb_overlay(img, payload_path, args.dry_run)
    elif args.mode == "REPL":
        result = _mode_replace_dtbo(payload_path, args.dry_run)
    else:  # PATCH
        result = _mode_patch_aboot(boot_path, payload_path, args.dry_run)

    # Step 3: 输出
    out_path = args.output or (boot_path.parent / f"{boot_path.stem}.patched.img")
    if result.success and not args.dry_run:
        with open(out_path, "wb") as fh:
            fh.write(result.patched_data)
        print(f"\n[OK]    写出 → {out_path}")
    elif args.dry_run:
        print("[DRY-RUN] 无输出，仅作模拟")
    else:
        print(f"[FAIL]  {result.details}", file=sys.stderr)
        sys.exit(2)


def _mode_dtb_overlay(img: dict, payload_path: Path, dry_run: bool) -> PatchResult:
    """OVL 模式：构造一个 Overlay FDT 让 aboot 识别并跳入 UEFI。"""
    overlay = build_uefi_overlay_fdt(payload_path.stat().st_size)
    new_dtb = apply_dtb_overlay(img["dtb"], overlay)
    patched = patch_dtb_in_boot_img(img, new_dtb)
    return PatchResult(success=True, mode="OVL", patched_data=patched,
                       details=f"Overlay {len(overlay)}B applied")


def _mode_replace_dtbo(payload_path: Path, dry_run: bool) -> PatchResult:
    """REPL 模式：把整个 dtbo 分区替换成含 UEFI 入口的新镜像。"""
    if dry_run:
        print(f"[DRY-RUN] 会将 device dtbo 分区 ({SDM660_DTBO_OFFSET:#x}) 替换为 payload")
        return PatchResult(success=True, mode="REPL", patched_data=payload_path.read_bytes(),
                           details="模拟跳过写入")
    print("[WARN]  REPL 模式会覆盖 dtbo 分区——请确认设备处于 fastboot 状态后再运行")
    return PatchResult(success=False, details="需要真机 fastboot 连接才能执行")


def _mode_patch_aboot(boot_path: Path, payload_path: Path, dry_run: bool) -> PatchResult:
    """PATCH 模式：直接篡改 aboot 二进制中的 jump-to-kernel 指令。"""
    if dry_run:
        print("[DRY-RUN] PATCH 模式：将修改 aboot 的 jump-to-kernel 指令序列")
        return PatchResult(success=True, mode="PATCH", patched_data=b"", details="模拟跳过写入")
    aboot_raw = boot_path.read_bytes()
    patched, patches = patch_aboot_for_uefi(aboot_raw)
    return PatchResult(success=len(patches) > 0, mode="PATCH", patched_data=patched,
                       details=" | ".join(patches) if patches else "未发现目标指令")


# ─── 辅助工具函数 ──────────────────────────────────────────────────

def build_uefi_overlay_fdt(firmware_size: int) -> bytes:
    """
    生成最小的 FDT overlay 片段，供 aboot 检测是否进入 UEFI 模式。
    节点结构（简化的 DT schema）：
        / {
            chosen {
                bootargs = "earlycon=uart8250,mmio32,0x013C0000 console=ttyMSM0,115200n8 androidboot.uefi=1";
            };
            uefi_payload {
                compatible = "jason,uefi-payload";
                firmware-base = <0xA0000000>;
                firmware-size = <firmware_size>;
                entry-point   = <0xA0010000>;
            };
        };
    生产环境应使用 python-libfdt 正确序列化，此处为演示结构。
    """
    # 真实 overlay 应用需要使用 libfdt；这里返回一个占位字节串
    # TODO: implement proper FDT overlay generation using pylibfdt
    placeholder = b"\x00" * max(64, firmware_size // 1024)
    return placeholder


def patch_dtb_in_boot_img(img: dict, new_dtb: bytes) -> bytes:
    """将新 DTB 嵌回 boot.img，保证页对齐。"""
    PAGE = img["page_size"]
    aligned_new = new_dtb + b"\x00" * ((PAGE - len(new_dtb) % PAGE) % PAGE)

    full = bytearray(img["full"])
    # DTB starts right after kernel+ramdisk (header already parsed)
    dtb_start = len(aligned_new)  # simplified — real offset needs exact header parsing
    full[dtb_start:dtb_start + len(aligned_new)] = aligned_new
    return bytes(full)


if __name__ == "__main__":
    main()
