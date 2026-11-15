#!/usr/bin/env python3
"""Extract the jason DTB from an Android boot.img without external tools.

The complete image is scanned for flattened device-tree blobs, including QCDT
blobs appended to Android v0 images where the boot header reports dt_size=0.
Candidates are validated structurally and selected by their root model string.
The input is only read; this tool does not repack, sign, patch, or flash it.

DTS output is generated when the optional pyfdt package is installed. DTB
extraction itself uses only the Python standard library.
"""

import argparse
from dataclasses import dataclass
import hashlib
import io
from pathlib import Path
import struct
import sys


FDT_MAGIC = b"\xd0\x0d\xfe\xed"
FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9


@dataclass(frozen=True)
class Candidate:
    offset: int
    size: int
    model: str


def align4(value: int) -> int:
    return (value + 3) & ~3


def cstring(data: bytes, start: int, limit: int) -> bytes:
    end = data.find(b"\0", start, limit)
    if end < 0:
        raise ValueError("unterminated device-tree string")
    return data[start:end]


def parse_candidate(image: bytes, offset: int) -> Candidate:
    if offset + 40 > len(image):
        raise ValueError("truncated FDT header")

    header = struct.unpack_from(">10I", image, offset)
    (
        magic,
        total_size,
        struct_offset,
        strings_offset,
        reserve_offset,
        version,
        last_compatible_version,
        _boot_cpu,
        strings_size,
        struct_size,
    ) = header
    if magic != struct.unpack(">I", FDT_MAGIC)[0]:
        raise ValueError("bad FDT magic")
    if not 16 <= version <= 17 or last_compatible_version > version:
        raise ValueError("unsupported FDT version")
    if total_size < 40 or offset + total_size > len(image):
        raise ValueError("FDT extends beyond the image")
    if struct_offset % 4:
        raise ValueError("unaligned FDT structure block")

    blob_end = offset + total_size
    struct_start = offset + struct_offset
    struct_end = struct_start + struct_size
    strings_start = offset + strings_offset
    strings_end = strings_start + strings_size
    if struct_end > blob_end or strings_end > blob_end:
        raise ValueError("invalid FDT block bounds")
    if reserve_offset > total_size or offset + reserve_offset + 16 > blob_end:
        raise ValueError("invalid FDT reserve map")

    position = struct_start
    depth = 0
    seen_root = False
    model = None
    while position + 4 <= struct_end:
        token = struct.unpack_from(">I", image, position)[0]
        position += 4

        if token == FDT_BEGIN_NODE:
            name = cstring(image, position, struct_end)
            if depth == 0:
                if seen_root or name:
                    raise ValueError("invalid FDT root node")
                seen_root = True
            depth += 1
            position = align4(position + len(name) + 1)
        elif token == FDT_END_NODE:
            if depth == 0:
                raise ValueError("unbalanced FDT nodes")
            depth -= 1
        elif token == FDT_PROP:
            if depth == 0 or position + 8 > struct_end:
                raise ValueError("invalid FDT property")
            value_size, name_offset = struct.unpack_from(">II", image, position)
            position += 8
            value_end = position + value_size
            if value_end > struct_end or name_offset >= strings_size:
                raise ValueError("FDT property extends beyond its block")
            name = cstring(image, strings_start + name_offset, strings_end)
            if depth == 1 and name == b"model":
                raw_model = image[position:value_end].rstrip(b"\0")
                model = raw_model.decode("utf-8", errors="replace")
            position = align4(value_end)
        elif token == FDT_NOP:
            pass
        elif token == FDT_END:
            if depth != 0 or not seen_root:
                raise ValueError("incomplete FDT")
            return Candidate(offset, total_size, model or "")
        else:
            raise ValueError("unknown FDT token")

    raise ValueError("missing FDT_END token")


def scan_dtbs(image: bytes) -> list[Candidate]:
    candidates = []
    offset = image.find(FDT_MAGIC)
    while offset >= 0:
        try:
            candidates.append(parse_candidate(image, offset))
        except (ValueError, struct.error):
            pass
        offset = image.find(FDT_MAGIC, offset + len(FDT_MAGIC))
    return candidates


def write_dts(dtb: bytes, output_path: Path) -> bool:
    try:
        from pyfdt.pyfdt import FdtBlobParse
    except ImportError:
        return False

    with io.BytesIO(dtb) as stream:
        dts = FdtBlobParse(stream).to_fdt().to_dts()
    with output_path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(dts)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot-img", required=True, type=Path, help="Original jason boot.img")
    parser.add_argument("--output", required=True, type=Path, help="Directory for extracted files")
    parser.add_argument(
        "--model",
        default="jason",
        help="Case-insensitive substring required in the DTB root model (default: jason)",
    )
    args = parser.parse_args()

    boot_img = args.boot_img.expanduser().resolve()
    output_dir = args.output.expanduser().resolve()
    if not boot_img.is_file():
        parser.error(f"boot image does not exist: {boot_img}")
    if boot_img == output_dir or output_dir in boot_img.parents:
        parser.error("--output must not contain the input boot image")

    try:
        image = boot_img.read_bytes()
        candidates = scan_dtbs(image)
        model_filter = args.model.casefold()
        matches = [item for item in candidates if model_filter in item.model.casefold()]
        print(f"[INFO] Structurally valid DTBs found: {len(candidates)}")
        if not matches:
            models = sorted({item.model for item in candidates if item.model})
            print("[ERROR] No DTB root model matches " + repr(args.model), file=sys.stderr)
            if models:
                print("[INFO] Available models include:", file=sys.stderr)
                for model in models[:10]:
                    print(f"  {model}", file=sys.stderr)
            return 1
        if len(matches) != 1:
            print("[ERROR] Model filter is not unique:", file=sys.stderr)
            for item in matches:
                print(f"  offset=0x{item.offset:X} model={item.model}", file=sys.stderr)
            return 1

        selected = matches[0]
        dtb = image[selected.offset:selected.offset + selected.size]
        output_dir.mkdir(parents=True, exist_ok=True)
        final_dtb = output_dir / "jason.dtb"
        final_dtb.write_bytes(dtb)

        print(f"[INFO] boot.img SHA-256: {hashlib.sha256(image).hexdigest().upper()}")
        print(f"[INFO] DTB offset: 0x{selected.offset:X}; size: {selected.size}")
        print(f"[INFO] Root model: {selected.model}")
        print(f"[OK] DTB: {final_dtb}")
        print(f"[OK] DTB SHA-256: {hashlib.sha256(dtb).hexdigest().upper()}")

        final_dts = output_dir / "jason.dts"
        if write_dts(dtb, final_dts):
            print(f"[OK] DTS: {final_dts}")
        else:
            print("[INFO] pyfdt is not installed; skipped DTS output (DTB extraction succeeded).")
    except (OSError, RuntimeError) as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1

    print("Input image was not modified. Keep this DTB tied to the boot image hash above.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
