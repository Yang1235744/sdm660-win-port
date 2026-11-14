#!/usr/bin/env python3
"""
gcc_tables.sh — Convenience wrapper around iASL (ACPI Source Language compiler)
to turn .asl source files into .aml blobs, then concatenate them into a single
firmware FV section that EDK II can embed.

Usage:
    ./gcc_tables.sh [--output <path>]
Defaults output to Build/JasonUefi/AcpiData/ under the EDK II WORKSPACE.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_iasl():
    """Look for iasl binary in PATH / common locations."""
    candidates = [
        shutil.which("iasl"),
        "/usr/bin/iasl",
        "/opt/acpica/bin/iasl",
        str(Path.home() / ".local" / "bin" / "iasl"),
    ]
    for c in candidates:
        if c and os.path.isfile(c):
            return c
    return None


def compile_asl(iasl: str, src: Path, out_dir: Path):
    """Compile one .asl file to .aml. Returns (aml_path, success)."""
    aml = out_dir / (src.stem + ".aml")
    result = subprocess.run(
        [iasl, "-tc", str(src)],
        capture_output=True, text=True, cwd=str(src.parent),
    )
    if result.returncode != 0:
        print(f"[ERROR] iasl failed for {src.name}:\n{result.stderr}", file=sys.stderr)
        return None, False
    if not aml.exists():
        print(f"[WARN]  iasl produced no .aml for {src.name}", file=sys.stderr)
        return None, False
    print(f"[OK]    compiled {src.name} -> {aml.name}")
    return aml, True


def main():
    parser = argparse.ArgumentParser(description="Build ACPI SSDT/RSDP blobs")
    parser.add_argument("--output", default=None, help="Output directory for .aml blobs")
    args = parser.parse_args()

    script_dir = Path(__file__).parent.resolve()
    src_files = sorted(script_dir.glob("*.asl"))
    if not src_files:
        print("[ERROR] No .asl sources found in this directory.", file=sys.stderr)
        sys.exit(1)

    out_dir = Path(args.output) if args.output else (script_dir / "build" / "acpi_blobs")
    out_dir.mkdir(parents=True, exist_ok=True)

    iasl = find_iasl()
    if not iasl:
        print("[ERROR] Could not find 'iasl' binary. Install ACPI Compiler from https://acpica.org/downloads",
              file=sys.stderr)
        sys.exit(1)
    print(f"[INFO] Using iASL at: {iasl}")

    aml_paths = []
    for src in src_files:
        aml, ok = compile_asl(iasl, src, out_dir)
        if ok:
            aml_paths.append(aml)

    if not aml_paths:
        print("[ERROR] No .aml files generated.", file=sys.stderr)
        sys.exit(1)

    # Print summary for downstream Makefile
    for p in aml_paths:
        size = p.stat().st_size
        print(f"[OUTPUT] {p}  ({size} bytes)")


if __name__ == "__main__":
    main()
