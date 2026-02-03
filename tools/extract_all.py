#!/usr/bin/env python3
"""
OpenSaints Batch Asset Extraction Tool
Extracts all assets from Saints Row 2 VPP archives with optional conversion.

Usage:
    python extract_all.py <game_dir> <output_dir> [options]

Options:
    --list              List contents without extracting
    --filter <pattern>  Only extract files matching pattern (glob-style)
    --type <ext>        Only extract files with this extension
    --convert-textures  Convert textures to PNG (requires Pillow)
    --convert-meshes    Convert meshes to OBJ
    --skip-existing     Skip files that already exist
    --verbose           Show detailed progress
"""

import struct
import os
import sys
import argparse
import glob
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import List, Dict, Tuple, Optional
import fnmatch
import time

# VPP format constants
VPP_MAGIC = 0x51890ACE
VPP_VERSION = 4
VPP_ALIGNMENT = 0x800

class VppEntry:
    """Represents a file entry in a VPP archive."""
    def __init__(self, filename: str, extension: str, data_offset: int, data_size: int):
        self.filename = filename
        self.extension = extension
        self.data_offset = data_offset
        self.data_size = data_size

    @property
    def full_name(self) -> str:
        if self.extension:
            return f"{self.filename}.{self.extension}"
        return self.filename

class VppArchive:
    """Parser for VPP_PC archive files."""

    def __init__(self, path: Path):
        self.path = path
        self.entries: List[VppEntry] = []
        self.file = None
        self._parse()

    def _align(self, offset: int) -> int:
        return ((offset + VPP_ALIGNMENT - 1) // VPP_ALIGNMENT) * VPP_ALIGNMENT

    def _parse(self):
        self.file = open(self.path, 'rb')

        # Read header
        header = self.file.read(0x180)
        magic, version = struct.unpack_from('<II', header, 0)

        if magic != VPP_MAGIC:
            raise ValueError(f"Invalid VPP magic: 0x{magic:08X}")
        if version != VPP_VERSION:
            print(f"Warning: VPP version {version} (expected {VPP_VERSION})")

        # Parse header fields at offset 0x14C
        num_files, container_size, len_offsets, len_filenames, len_extensions = \
            struct.unpack_from('<iiiii', header, 0x14C)

        # Read file entries starting at 0x800
        self.file.seek(VPP_ALIGNMENT)
        entries_data = self.file.read(num_files * 28)

        # Calculate section offsets
        offsets_end = VPP_ALIGNMENT + len_offsets
        filenames_start = self._align(offsets_end)
        filenames_end = filenames_start + len_filenames
        extensions_start = self._align(filenames_end)
        extensions_end = extensions_start + len_extensions
        data_start = self._align(extensions_end)

        # Read filename section
        self.file.seek(filenames_start)
        filenames_section = self.file.read(len_filenames)

        # Read extensions section
        self.file.seek(extensions_start)
        extensions_section = self.file.read(len_extensions)

        # Parse entries
        for i in range(num_files):
            entry_data = entries_data[i * 28:(i + 1) * 28]
            name_offset, ext_offset, _, data_offset, data_size, _, _ = \
                struct.unpack('<IIiiiII', entry_data)

            # Extract strings
            filename = self._read_string(filenames_section, name_offset)
            extension = self._read_string(extensions_section, ext_offset)

            self.entries.append(VppEntry(
                filename=filename,
                extension=extension,
                data_offset=data_start + data_offset,
                data_size=data_size
            ))

    def _read_string(self, section: bytes, offset: int) -> str:
        if offset >= len(section):
            return ""
        end = section.find(b'\0', offset)
        if end == -1:
            end = len(section)
        return section[offset:end].decode('utf-8', errors='replace')

    def extract(self, entry: VppEntry) -> bytes:
        """Extract a single file's data."""
        self.file.seek(entry.data_offset)
        return self.file.read(entry.data_size)

    def extract_to(self, entry: VppEntry, output_path: Path) -> bool:
        """Extract a single file to disk."""
        try:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            data = self.extract(entry)
            output_path.write_bytes(data)
            return True
        except Exception as e:
            print(f"Error extracting {entry.full_name}: {e}")
            return False

    def close(self):
        if self.file:
            self.file.close()
            self.file = None

def find_vpp_files(game_dir: Path) -> List[Path]:
    """Find all VPP files in the game directory."""
    return list(game_dir.glob("*.vpp_pc"))

def extract_archive(vpp_path: Path, output_dir: Path, args) -> Dict:
    """Extract a single VPP archive."""
    stats = {
        'archive': vpp_path.name,
        'total': 0,
        'extracted': 0,
        'skipped': 0,
        'errors': 0,
        'bytes': 0
    }

    try:
        archive = VppArchive(vpp_path)
        stats['total'] = len(archive.entries)

        archive_output = output_dir / vpp_path.stem

        for entry in archive.entries:
            # Check filter
            if args.filter and not fnmatch.fnmatch(entry.full_name.lower(), args.filter.lower()):
                stats['skipped'] += 1
                continue

            # Check type filter
            if args.type and entry.extension.lower() != args.type.lower():
                stats['skipped'] += 1
                continue

            output_path = archive_output / entry.full_name

            # Check if exists
            if args.skip_existing and output_path.exists():
                stats['skipped'] += 1
                continue

            if args.list:
                print(f"  {entry.full_name} ({entry.data_size:,} bytes)")
            else:
                if archive.extract_to(entry, output_path):
                    stats['extracted'] += 1
                    stats['bytes'] += entry.data_size

                    if args.verbose:
                        print(f"  Extracted: {entry.full_name}")
                else:
                    stats['errors'] += 1

        archive.close()

    except Exception as e:
        print(f"Error processing {vpp_path.name}: {e}")
        stats['errors'] += 1

    return stats

def convert_texture_to_png(peg_path: Path, output_dir: Path) -> bool:
    """Convert a PEG texture to PNG format."""
    try:
        from PIL import Image
        # Placeholder - actual implementation would parse PEG format
        print(f"Texture conversion not yet implemented: {peg_path.name}")
        return False
    except ImportError:
        print("Pillow not installed. Run: pip install Pillow")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="OpenSaints Batch Asset Extraction Tool"
    )
    parser.add_argument('game_dir', type=Path, help="Saints Row 2 game directory")
    parser.add_argument('output_dir', type=Path, help="Output directory for extracted files")
    parser.add_argument('--list', action='store_true', help="List contents without extracting")
    parser.add_argument('--filter', type=str, help="Filter files by pattern (glob-style)")
    parser.add_argument('--type', type=str, help="Filter by file extension")
    parser.add_argument('--convert-textures', action='store_true', help="Convert textures to PNG")
    parser.add_argument('--convert-meshes', action='store_true', help="Convert meshes to OBJ")
    parser.add_argument('--skip-existing', action='store_true', help="Skip existing files")
    parser.add_argument('--verbose', '-v', action='store_true', help="Verbose output")
    parser.add_argument('--parallel', type=int, default=4, help="Number of parallel workers")

    args = parser.parse_args()

    if not args.game_dir.exists():
        print(f"Error: Game directory not found: {args.game_dir}")
        sys.exit(1)

    # Find VPP files
    vpp_files = find_vpp_files(args.game_dir)
    if not vpp_files:
        print(f"No VPP files found in: {args.game_dir}")
        sys.exit(1)

    print(f"Found {len(vpp_files)} VPP archives:")
    for vpp in sorted(vpp_files):
        size_mb = vpp.stat().st_size / (1024 * 1024)
        print(f"  {vpp.name} ({size_mb:.1f} MB)")
    print()

    if not args.list:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        print(f"Extracting to: {args.output_dir}")

    # Process archives
    start_time = time.time()
    total_stats = {
        'archives': len(vpp_files),
        'total': 0,
        'extracted': 0,
        'skipped': 0,
        'errors': 0,
        'bytes': 0
    }

    for vpp_path in sorted(vpp_files):
        print(f"\nProcessing: {vpp_path.name}")
        stats = extract_archive(vpp_path, args.output_dir, args)

        total_stats['total'] += stats['total']
        total_stats['extracted'] += stats['extracted']
        total_stats['skipped'] += stats['skipped']
        total_stats['errors'] += stats['errors']
        total_stats['bytes'] += stats['bytes']

        if not args.list:
            print(f"  {stats['extracted']:,} extracted, {stats['skipped']:,} skipped, {stats['errors']} errors")

    elapsed = time.time() - start_time

    print(f"\n{'='*60}")
    print(f"Summary:")
    print(f"  Archives processed: {total_stats['archives']}")
    print(f"  Total files: {total_stats['total']:,}")
    print(f"  Extracted: {total_stats['extracted']:,}")
    print(f"  Skipped: {total_stats['skipped']:,}")
    print(f"  Errors: {total_stats['errors']}")
    print(f"  Total size: {total_stats['bytes'] / (1024*1024):.1f} MB")
    print(f"  Time: {elapsed:.1f} seconds")

    if total_stats['extracted'] > 0:
        speed = total_stats['bytes'] / (1024*1024) / elapsed
        print(f"  Speed: {speed:.1f} MB/s")

if __name__ == '__main__':
    main()
