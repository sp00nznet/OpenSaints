# OpenSaints

A clean-room reimplementation of the Saints Row 2 engine for modern systems.

```
   ____                   _____       _       _
  / __ \                 / ____|     (_)     | |
 | |  | |_ __   ___ _ __| (___   __ _ _ _ __ | |_ ___
 | |  | | '_ \ / _ \ '_ \\___ \ / _` | | '_ \| __/ __|
 | |__| | |_) |  __/ | | |___) | (_| | | | | | |_\__ \
  \____/| .__/ \___|_| |_|____/ \__,_|_|_| |_|\__|___/
        | |
        |_|
```

## Overview

OpenSaints is a project to create an open-source engine compatible with Saints Row 2 assets. It allows the game to run on modern systems (Windows, Linux, macOS) with improved stability, debugging capabilities, and modding support.

**This project requires a legal copy of Saints Row 2 to function.** OpenSaints does not include any game assets.

## Features

### Implemented (Phase 1)
- [x] VPP archive extraction and virtual filesystem
- [x] Preload table parsing for asset dependencies
- [x] XTBL configuration file parsing
- [x] PEG texture package parsing with DXT decompression
- [x] Mesh parsing (character and static meshes)
- [x] World chunk parsing
- [x] Asset manager with caching and memory management

### In Progress (Phase 2)
- [ ] Vulkan rendering backend
- [ ] SDL2 window/input handling
- [ ] World streaming system
- [ ] Basic mesh rendering

### Planned
- [ ] Animation system
- [ ] Audio system (OpenAL)
- [ ] Physics integration
- [ ] Script system
- [ ] Full game logic

## Building

### Requirements
- CMake 3.16+
- C++20 compiler (MSVC 2019+, GCC 10+, Clang 10+)
- Optional: SDL2, Vulkan SDK, GLM (for renderer)

### Quick Start

```bash
# Clone the repository
git clone https://github.com/yourusername/OpenSaints.git
cd OpenSaints

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run (point to your SR2 installation)
./build/opensaints "C:/Games/Saints Row 2"
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TOOLS` | ON | Build asset extraction tools |
| `BUILD_TESTS` | OFF | Build unit tests |
| `BUILD_RENDERER` | OFF | Build rendering engine (requires SDL2, Vulkan) |

## Usage

### Main Application
```bash
# Show game asset information
opensaints --info "path/to/saints row 2"

# List all assets by type
opensaints --list-assets "path/to/saints row 2"
```

### Asset Extraction (Python)
```bash
# Extract all assets
python tools/extract_all.py "path/to/saints row 2" ./extracted

# Extract only textures
python tools/extract_all.py "path/to/saints row 2" ./extracted --type cpeg_pc

# List contents without extracting
python tools/extract_all.py "path/to/saints row 2" ./extracted --list
```

### VPP Extraction (C++)
```bash
# Extract a single archive
vpp_extract common.vpp_pc ./common_extracted
```

## Project Structure

```
OpenSaints/
├── src/
│   ├── formats/          # File format parsers
│   │   ├── vpp.*         # VPP archive format
│   │   ├── peg.*         # Texture packages
│   │   ├── mesh.*        # Character/static meshes
│   │   ├── chunk.*       # World chunks
│   │   ├── xtbl.*        # XML config tables
│   │   └── preload_table.*
│   ├── engine/           # Core engine systems
│   │   ├── vfs.*         # Virtual filesystem
│   │   └── asset_manager.*
│   ├── render/           # Rendering (planned)
│   ├── audio/            # Audio system (planned)
│   └── world/            # World/streaming (planned)
├── tools/                # Utility tools
│   ├── vpp_extract.*     # VPP extraction
│   ├── extract_all.py    # Batch extraction
│   └── pe_analyze.py     # Executable analysis
└── docs/                 # Documentation
```

## Documentation

- [Architecture Overview](docs/architecture.md)
- [File Formats Reference](docs/formats.md)
- [Building & Development](docs/building.md)
- [Contributing Guide](docs/contributing.md)

## Technical Details

### Supported Formats

| Extension | Type | Status |
|-----------|------|--------|
| `.vpp_pc` | Archive | ✅ Full support |
| `.cpeg_pc`/`.gpeg_pc` | Textures | ✅ Full support |
| `.cvbm_pc`/`.gvbm_pc` | Textures | ✅ Full support |
| `.cmesh_pc`/`.gcmesh_pc` | Character mesh | 🔄 Basic support |
| `.smesh_pc`/`.gsmesh_pc` | Static mesh | 🔄 Basic support |
| `.chunk_pc` | World geometry | 🔄 Basic support |
| `.xtbl` | Config tables | ✅ Full support |
| `.anim_pc` | Animations | ⏳ Planned |
| `.vint_doc` | UI documents | ⏳ Planned |

### Engine Architecture

OpenSaints uses a modular architecture:

- **Virtual Filesystem**: Mounts VPP archives and provides unified file access
- **Asset Manager**: Handles loading, caching, and memory management
- **Format Parsers**: Convert game formats to engine-friendly structures
- **Renderer**: Vulkan-based rendering (in development)

## Legal

This project is a clean-room reimplementation. It does not contain any code or assets from the original game. You must own a legal copy of Saints Row 2 to use this software.

**References used (documentation only):**
- Kaitai Struct format specifications
- Community format documentation (volition-docs)
- Public game analysis (TCRF)

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](docs/contributing.md) before submitting PRs.

Areas where help is needed:
- Format reverse engineering
- Vulkan rendering implementation
- Cross-platform testing
- Documentation

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- The Saints Row modding community for format documentation
- Volition for creating Saints Row 2
- Contributors to open-source game reimplementation projects

---

*OpenSaints is not affiliated with Volition, Deep Silver, or any other rights holders of Saints Row.*
