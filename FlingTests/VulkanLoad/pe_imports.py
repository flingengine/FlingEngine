#!/usr/bin/env python3
"""Print the import and delay-import DLL names of a PE binary, one per line."""

from __future__ import annotations

import struct
import sys
from pathlib import Path


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _sections(data: bytes, offset: int, count: int) -> list[tuple[int, int, int]]:
    sections: list[tuple[int, int, int]] = []
    for index in range(count):
        base = offset + index * 40
        virtual_address = _u32(data, base + 12)
        raw_size = _u32(data, base + 16)
        raw_pointer = _u32(data, base + 20)
        sections.append((virtual_address, raw_pointer, raw_size))
    return sections


def _rva_to_offset(rva: int, sections: list[tuple[int, int, int]]) -> int | None:
    for virtual_address, raw_pointer, raw_size in sections:
        if raw_size and virtual_address <= rva < virtual_address + raw_size:
            return raw_pointer + (rva - virtual_address)
    return None


def _cstring(data: bytes, offset: int) -> str:
    end = data.find(b"\x00", offset)
    if end < 0:
        raise ValueError("DLL name is not terminated")
    return data[offset:end].decode("ascii", "replace")


def import_names(path: Path) -> list[str]:
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("not a PE file")
    pe = _u32(data, 0x3C)
    if data[pe : pe + 4] != b"PE\x00\x00":
        raise ValueError("bad PE signature")

    coff = pe + 4
    section_count = _u16(data, coff + 2)
    optional_size = _u16(data, coff + 16)
    optional = coff + 20
    magic = _u16(data, optional)
    if magic == 0x20B:
        directory_count = _u32(data, optional + 108)
        directory = optional + 112
    elif magic == 0x10B:
        directory_count = _u32(data, optional + 92)
        directory = optional + 96
    else:
        raise ValueError(f"unknown PE magic {magic:#x}")

    sections = _sections(data, optional + optional_size, section_count)
    names: list[str] = []

    def directory_entry(index: int) -> tuple[int, int]:
        if index >= directory_count:
            return 0, 0
        base = directory + index * 8
        return _u32(data, base), _u32(data, base + 4)

    import_rva, import_size = directory_entry(1)
    if import_rva and import_size:
        offset = _rva_to_offset(import_rva, sections)
        if offset is None:
            raise ValueError("import directory RVA is outside the file")
        while True:
            original, _time, _forwarder, name_rva, first_thunk = struct.unpack_from("<IIIII", data, offset)
            if original == 0 and name_rva == 0 and first_thunk == 0:
                break
            name_offset = _rva_to_offset(name_rva, sections)
            if name_offset is None:
                raise ValueError("import DLL name RVA is outside the file")
            names.append(_cstring(data, name_offset))
            offset += 20

    # MSVC delay-load descriptors. grAttrs bit 0 means the fields are RVAs.
    delay_rva, delay_size = directory_entry(13)
    if delay_rva and delay_size:
        offset = _rva_to_offset(delay_rva, sections)
        if offset is None:
            raise ValueError("delay-import directory RVA is outside the file")
        while offset + 32 <= len(data):
            attrs, name_rva, _hmod, _iat, _int, _bound, _unload, _stamp = struct.unpack_from("<IIIIIIII", data, offset)
            if attrs == 0 and name_rva == 0:
                break
            if attrs & 1:
                name_offset = _rva_to_offset(name_rva, sections)
            else:
                name_offset = name_rva if name_rva < len(data) else None
            if name_offset is None:
                raise ValueError("delay-import DLL name is outside the file")
            names.append(_cstring(data, name_offset))
            offset += 32

    return names


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: pe_imports.py <binary>", file=sys.stderr)
        return 2
    try:
        for name in import_names(Path(sys.argv[1])):
            print(name)
    except (OSError, ValueError, struct.error) as error:
        print(f"pe_imports: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
