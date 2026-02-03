#!/usr/bin/env python3
"""
Simple PE file analyzer for Saints Row 2 executable
Extracts useful information for reverse engineering
"""

import struct
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import List, Dict, Tuple


@dataclass
class PESection:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int
    characteristics: int


@dataclass
class ImportEntry:
    dll_name: str
    functions: List[str]


class PEAnalyzer:
    def __init__(self, path: Path):
        self.path = path
        self.data = b''
        self.dos_header = {}
        self.pe_header = {}
        self.optional_header = {}
        self.sections: List[PESection] = []
        self.imports: List[ImportEntry] = []

    def load(self) -> bool:
        with open(self.path, 'rb') as f:
            self.data = f.read()

        # DOS Header
        if self.data[:2] != b'MZ':
            print("Not a valid PE file (missing MZ)")
            return False

        self.dos_header['e_lfanew'] = struct.unpack_from('<I', self.data, 0x3C)[0]

        # PE Signature
        pe_offset = self.dos_header['e_lfanew']
        if self.data[pe_offset:pe_offset+4] != b'PE\x00\x00':
            print("Not a valid PE file (missing PE signature)")
            return False

        # COFF Header
        coff_offset = pe_offset + 4
        (machine, num_sections, timestamp, sym_table, num_symbols,
         opt_header_size, characteristics) = struct.unpack_from('<HHIIIHH', self.data, coff_offset)

        self.pe_header = {
            'machine': machine,
            'num_sections': num_sections,
            'timestamp': timestamp,
            'opt_header_size': opt_header_size,
            'characteristics': characteristics,
        }

        # Optional Header (PE32)
        opt_offset = coff_offset + 20
        (magic, major_linker, minor_linker, code_size, init_data, uninit_data,
         entry_point, code_base, data_base, image_base) = struct.unpack_from('<HBBIIIIIII', self.data, opt_offset)

        self.optional_header = {
            'magic': magic,
            'entry_point': entry_point,
            'code_base': code_base,
            'image_base': image_base,
        }

        # Section alignment, file alignment, etc.
        (section_align, file_align, os_major, os_minor, img_major, img_minor,
         subsys_major, subsys_minor, reserved, image_size, header_size,
         checksum, subsystem, dll_chars) = struct.unpack_from('<IIHHHHHIIIIIHH', self.data, opt_offset + 32)

        self.optional_header['section_alignment'] = section_align
        self.optional_header['file_alignment'] = file_align
        self.optional_header['image_size'] = image_size
        self.optional_header['subsystem'] = subsystem

        # Data directories (32-bit PE has 16)
        dd_offset = opt_offset + 96
        data_dirs = []
        for i in range(16):
            rva, size = struct.unpack_from('<II', self.data, dd_offset + i * 8)
            data_dirs.append((rva, size))

        self.optional_header['data_directories'] = data_dirs

        # Section headers
        section_offset = opt_offset + opt_header_size
        for i in range(num_sections):
            sec_data = self.data[section_offset + i * 40:section_offset + (i + 1) * 40]
            name = sec_data[:8].rstrip(b'\x00').decode('ascii', errors='replace')
            (virt_size, virt_addr, raw_size, raw_offset,
             reloc_ptr, linenum_ptr, num_relocs, num_lines, chars) = struct.unpack_from('<IIIIIIHHI', sec_data, 8)

            self.sections.append(PESection(
                name=name,
                virtual_size=virt_size,
                virtual_address=virt_addr,
                raw_size=raw_size,
                raw_offset=raw_offset,
                characteristics=chars
            ))

        # Parse imports
        import_rva, import_size = data_dirs[1]  # Import directory
        if import_rva > 0:
            self._parse_imports(import_rva)

        return True

    def _rva_to_offset(self, rva: int) -> int:
        """Convert RVA to file offset."""
        for sec in self.sections:
            if sec.virtual_address <= rva < sec.virtual_address + sec.virtual_size:
                return rva - sec.virtual_address + sec.raw_offset
        return rva

    def _parse_imports(self, import_rva: int):
        """Parse import directory."""
        offset = self._rva_to_offset(import_rva)

        while True:
            # Import descriptor: OriginalFirstThunk, TimeDateStamp, ForwarderChain, Name, FirstThunk
            (oft, ts, fc, name_rva, ft) = struct.unpack_from('<IIIII', self.data, offset)
            offset += 20

            if name_rva == 0:
                break

            name_offset = self._rva_to_offset(name_rva)
            dll_name = self._read_cstring(name_offset)

            # Read function names from OriginalFirstThunk (or FirstThunk if OFT is 0)
            thunk_rva = oft if oft != 0 else ft
            thunk_offset = self._rva_to_offset(thunk_rva)

            functions = []
            while True:
                thunk = struct.unpack_from('<I', self.data, thunk_offset)[0]
                thunk_offset += 4

                if thunk == 0:
                    break

                if thunk & 0x80000000:  # Ordinal import
                    functions.append(f"Ordinal_{thunk & 0xFFFF}")
                else:
                    hint_name_offset = self._rva_to_offset(thunk)
                    func_name = self._read_cstring(hint_name_offset + 2)  # Skip hint
                    functions.append(func_name)

            self.imports.append(ImportEntry(dll_name=dll_name, functions=functions))

    def _read_cstring(self, offset: int) -> str:
        """Read null-terminated string."""
        end = self.data.find(b'\x00', offset)
        if end == -1:
            end = offset + 256
        return self.data[offset:end].decode('ascii', errors='replace')

    def find_strings(self, min_length: int = 4) -> List[Tuple[int, str]]:
        """Find ASCII strings in the executable."""
        strings = []
        current = b''
        start = 0

        for i, byte in enumerate(self.data):
            if 0x20 <= byte < 0x7F:
                if not current:
                    start = i
                current += bytes([byte])
            else:
                if len(current) >= min_length:
                    strings.append((start, current.decode('ascii')))
                current = b''

        return strings

    def print_summary(self):
        """Print analysis summary."""
        print(f"PE Analysis: {self.path.name}")
        print("=" * 60)

        print(f"\nFile size: {len(self.data):,} bytes")

        # Machine type
        machine = self.pe_header['machine']
        machine_str = {0x14c: 'i386', 0x8664: 'AMD64'}.get(machine, f'0x{machine:04X}')
        print(f"Machine: {machine_str}")

        # Entry point
        entry = self.optional_header['entry_point']
        image_base = self.optional_header['image_base']
        print(f"Image base: 0x{image_base:08X}")
        print(f"Entry point: 0x{entry:08X} (VA: 0x{image_base + entry:08X})")

        print(f"\nSections ({len(self.sections)}):")
        print("-" * 60)
        print(f"{'Name':<10} {'VirtAddr':>10} {'VirtSize':>10} {'RawSize':>10} {'Chars':>10}")
        for sec in self.sections:
            print(f"{sec.name:<10} 0x{sec.virtual_address:08X} 0x{sec.virtual_size:08X} "
                  f"0x{sec.raw_size:08X} 0x{sec.characteristics:08X}")

        print(f"\nImports ({len(self.imports)} DLLs):")
        print("-" * 60)
        for imp in self.imports:
            print(f"  {imp.dll_name} ({len(imp.functions)} functions)")

    def print_imports_detail(self):
        """Print detailed import list."""
        print("\nDetailed Imports:")
        print("=" * 60)
        for imp in self.imports:
            print(f"\n{imp.dll_name}:")
            for func in imp.functions:
                print(f"    {func}")

    def export_docs(self, output_path: Path):
        """Export analysis to documentation file."""
        with open(output_path, 'w') as f:
            f.write(f"# SR2_pc.exe Analysis\n\n")
            f.write(f"## Overview\n\n")
            f.write(f"- File size: {len(self.data):,} bytes\n")
            f.write(f"- Architecture: {'32-bit' if self.pe_header['machine'] == 0x14c else '64-bit'}\n")
            f.write(f"- Image base: 0x{self.optional_header['image_base']:08X}\n")
            f.write(f"- Entry point: 0x{self.optional_header['entry_point']:08X}\n\n")

            f.write(f"## Sections\n\n")
            f.write("| Name | Virtual Address | Virtual Size | Raw Size | Characteristics |\n")
            f.write("|------|-----------------|--------------|----------|----------------|\n")
            for sec in self.sections:
                f.write(f"| {sec.name} | 0x{sec.virtual_address:08X} | "
                       f"0x{sec.virtual_size:08X} | 0x{sec.raw_size:08X} | 0x{sec.characteristics:08X} |\n")

            f.write(f"\n## Imports\n\n")
            for imp in self.imports:
                f.write(f"### {imp.dll_name}\n\n")
                for func in imp.functions:
                    f.write(f"- {func}\n")
                f.write("\n")

            f.write(f"\n## Known Addresses (from Monkey Patch)\n\n")
            f.write("| Address | Description |\n")
            f.write("|---------|-------------|\n")
            f.write("| 0x00520ba0 | WinMain function |\n")
            f.write("| 0x00c9e1c0 | CRT startup hook point |\n")

        print(f"Documentation exported to: {output_path}")


def main():
    if len(sys.argv) < 2:
        print("PE Analyzer for Saints Row 2")
        print(f"Usage: {sys.argv[0]} <exe_path> [--detail] [--export <output.md>]")
        sys.exit(1)

    exe_path = Path(sys.argv[1])
    show_detail = '--detail' in sys.argv
    export_path = None

    if '--export' in sys.argv:
        idx = sys.argv.index('--export')
        if idx + 1 < len(sys.argv):
            export_path = Path(sys.argv[idx + 1])

    analyzer = PEAnalyzer(exe_path)
    if not analyzer.load():
        sys.exit(1)

    analyzer.print_summary()

    if show_detail:
        analyzer.print_imports_detail()

    if export_path:
        analyzer.export_docs(export_path)


if __name__ == '__main__':
    main()
