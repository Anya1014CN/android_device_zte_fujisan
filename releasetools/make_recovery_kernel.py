#!/usr/bin/env python3
"""Assemble Fujisan's recovery kernel with the known-good TWRP DTBs.

The Lineage recovery ramdisk needs a native panel-A framebuffer, while Android
needs the current kernel's dual-screen DTBs. Preserve the freshly built
Image.gz byte-for-byte and replace only its appended FDTs with the matching
DTBs extracted from the known-working TWRP recovery image.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9
MSM_ID_PROPERTY = b"qcom,msm-id"


def align(value: int) -> int:
    return (value + 3) & ~3


def fdt_offsets(blob: bytes) -> list[tuple[int, int]]:
    result: list[tuple[int, int]] = []
    cursor = 0
    magic = struct.pack(">I", FDT_MAGIC)
    while True:
        position = blob.find(magic, cursor)
        if position < 0:
            return result
        if position + 40 <= len(blob):
            _, total, off_struct, off_strings, _, _, _, _, strings_size, struct_size = struct.unpack_from(
                ">10I", blob, position
            )
            if (total >= 40 and position + total <= len(blob) and 40 <= off_struct < total
                    and 40 <= off_strings < total and struct_size <= total - off_struct
                    and strings_size <= total - off_strings):
                result.append((position, total))
                cursor = position + total
                continue
        cursor = position + 4


def root_msm_id(dtb: bytes) -> bytes:
    _, _, off_struct, off_strings, _, _, _, _, strings_size, struct_size = struct.unpack_from(
        ">10I", dtb
    )
    cursor = off_struct
    end = off_struct + struct_size
    depth = 0
    while cursor < end:
        token = struct.unpack_from(">I", dtb, cursor)[0]
        if token == FDT_BEGIN_NODE:
            name_end = dtb.find(b"\0", cursor + 4, end)
            if name_end < 0:
                raise ValueError("unterminated FDT node")
            depth += 1
            cursor = align(name_end + 1)
        elif token == FDT_END_NODE:
            depth -= 1
            cursor += 4
        elif token == FDT_PROP:
            length, name_off = struct.unpack_from(">II", dtb, cursor + 4)
            data_start = cursor + 12
            next_cursor = align(data_start + length)
            if next_cursor > end or name_off >= strings_size:
                raise ValueError("invalid FDT property")
            name_start = off_strings + name_off
            name_end = dtb.find(b"\0", name_start, off_strings + strings_size)
            if name_end < 0:
                raise ValueError("unterminated FDT property name")
            if depth == 1 and dtb[name_start:name_end] == MSM_ID_PROPERTY:
                return dtb[data_start:data_start + length]
            cursor = next_cursor
        elif token == FDT_NOP:
            cursor += 4
        elif token in (FDT_END, 0):
            break
        else:
            raise ValueError(f"unknown FDT token {token} at 0x{cursor:x}")
    raise ValueError("qcom,msm-id missing from FDT root")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dtb-dir", type=Path, required=True)
    args = parser.parse_args()

    image = args.input.read_bytes()
    offsets = fdt_offsets(image)
    if not offsets:
        raise SystemExit("no appended FDTs found in kernel image")

    replacements = []
    for offset, size in offsets:
        ident = root_msm_id(image[offset:offset + size])
        reference = args.dtb_dir / f"{ident.hex()}.dtb"
        if not reference.is_file():
            raise SystemExit(f"missing TWRP DTB for qcom,msm-id={ident.hex()}")
        replacements.append(reference.read_bytes())

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image[:offsets[0][0]] + b"".join(replacements))
    print(f"prepared recovery kernel: substituted {len(replacements)} TWRP DTBs")


if __name__ == "__main__":
    main()
