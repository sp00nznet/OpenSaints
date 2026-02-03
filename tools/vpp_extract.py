#!/usr/bin/env python3
"""
VPP_PC file extractor for Saints Row 2
Based on Kaitai Struct specification

Usage: python vpp_extract.py <input.vpp_pc> [output_dir]
"""

import struct
import sys
import os
from pathlib import Path
from dataclasses import dataclass
from typing import List, BinaryIO

# Constants
VPP_MAGIC = 0x51890ACE
VPP_VERSION = 0x04
VPP_ALIGNMENT = 0x800  # 2048 bytes


@dataclass
class VppFileEntry:
    name_offset: int
    ext_offset: int
    unknown: int
    data_offset: int
    data_size: int
    always_minus1: int
    always_zero: int


@dataclass
class VppFileInfo:
    filename: str
    extension: str
    data_offset: int  # Absolute offset
    data_size: int

    @property
    def full_name(self) -> str:
        if self.extension:
            return f"{self.filename}.{self.extension}"
        return self.filename


def align_to(offset: int, alignment: int = VPP_ALIGNMENT) -> int:
    """Align offset to boundary."""
    return ((offset + alignment - 1) // alignment) * alignment


class VppArchive:
    def __init__(self, path: Path):
        self.path = path
        self.file: BinaryIO = None
        self.files: List[VppFileInfo] = []
        self.num_files = 0

    def open(self) -> bool:
        """Open and parse the VPP archive."""
        self.file = open(self.path, 'rb')

        # Read header
        header = self.file.read(0x800)
        magic, version = struct.unpack_from('<II', header, 0)

        if magic != VPP_MAGIC:
            print(f"Invalid VPP magic: 0x{magic:08X} (expected 0x{VPP_MAGIC:08X})")
            return False

        if version != VPP_VERSION:
            print(f"Warning: VPP version {version} (expected {VPP_VERSION})")

        # Parse header fields at offset 0x154
        (self.num_files, container_size, len_offsets,
         len_filenames, len_extensions) = struct.unpack_from('<5i', header, 0x154)

        print(f"Archive: {self.path.name}")
        print(f"  Files: {self.num_files}")
        print(f"  Offsets size: {len_offsets}")
        print(f"  Filenames size: {len_filenames}")
        print(f"  Extensions size: {len_extensions}")

        # Read file entries (starting at 0x800)
        self.file.seek(VPP_ALIGNMENT)
        entries = []
        for _ in range(self.num_files):
            entry_data = self.file.read(28)  # 7 * 4 bytes
            entry = VppFileEntry(*struct.unpack('<IIiiiii', entry_data))
            entries.append(entry)

        # Calculate section offsets
        offsets_end = VPP_ALIGNMENT + len_offsets
        filenames_start = align_to(offsets_end)
        filenames_end = filenames_start + len_filenames
        extensions_start = align_to(filenames_end)
        extensions_end = extensions_start + len_extensions
        data_start = align_to(extensions_end)

        # Read filename section
        self.file.seek(filenames_start)
        filenames_data = self.file.read(len_filenames)

        # Read extension section
        self.file.seek(extensions_start)
        extensions_data = self.file.read(len_extensions)

        # Build file info list
        self.files = []
        for entry in entries:
            # Extract null-terminated strings
            filename = self._read_cstring(filenames_data, entry.name_offset)
            extension = self._read_cstring(extensions_data, entry.ext_offset)

            info = VppFileInfo(
                filename=filename,
                extension=extension,
                data_offset=data_start + entry.data_offset,
                data_size=entry.data_size
            )
            self.files.append(info)

        return True

    def _read_cstring(self, data: bytes, offset: int) -> str:
        """Read a null-terminated string from data."""
        if offset >= len(data):
            return ""
        end = data.find(b'\x00', offset)
        if end == -1:
            end = len(data)
        return data[offset:end].decode('ascii', errors='replace')

    def extract(self, index: int) -> bytes:
        """Extract a file by index."""
        if index >= len(self.files):
            return b''
        info = self.files[index]
        self.file.seek(info.data_offset)
        return self.file.read(info.data_size)

    def extract_all(self, output_dir: Path) -> bool:
        """Extract all files to directory."""
        output_dir.mkdir(parents=True, exist_ok=True)

        for i, info in enumerate(self.files):
            out_path = output_dir / info.full_name

            # Handle subdirectories in filename
            out_path.parent.mkdir(parents=True, exist_ok=True)

            data = self.extract(i)
            with open(out_path, 'wb') as f:
                f.write(data)

            if (i + 1) % 100 == 0 or i == len(self.files) - 1:
                print(f"  Extracted {i + 1}/{len(self.files)} files...")

        print(f"Done! Extracted {len(self.files)} files to {output_dir}")
        return True

    def list_files(self):
        """Print file listing."""
        print(f"\nContents of {self.path.name}:")
        print("-" * 60)
        for i, info in enumerate(self.files):
            print(f"  [{i:4d}] {info.full_name} ({info.data_size:,} bytes)")
        print("-" * 60)
        print(f"Total: {len(self.files)} files\n")

    def close(self):
        if self.file:
            self.file.close()
            self.file = None


def main():
    if len(sys.argv) < 2:
        print("OpenSaints VPP Extractor")
        print(f"Usage: {sys.argv[0]} <input.vpp_pc> [output_dir]")
        print(f"       {sys.argv[0]} <input.vpp_pc> --list")
        sys.exit(1)

    input_path = Path(sys.argv[1])
    list_only = '--list' in sys.argv

    if len(sys.argv) >= 3 and sys.argv[2] != '--list':
        output_dir = Path(sys.argv[2])
    else:
        output_dir = Path(f"{input_path.stem}_extracted")

    archive = VppArchive(input_path)
    if not archive.open():
        sys.exit(1)

    archive.list_files()

    if not list_only:
        print(f"Extracting to: {output_dir}")
        archive.extract_all(output_dir)

    archive.close()


if __name__ == '__main__':
    main()
