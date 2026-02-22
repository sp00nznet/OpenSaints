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

## Current Status

**Early Development** - Asset pipeline complete, rendering integration in progress.

See [docs/development-status.md](docs/development-status.md) for detailed status.

### Working
- [x] Vulkan rendering backend with basic geometry
- [x] SDL2 window/input handling
- [x] VPP archive extraction (Python and C++ tools)
- [x] PEG texture parsing and DXT decompression (disk + memory/VFS)
- [x] Texture export to BMP/TGA
- [x] Demo scene with rotating objects
- [x] CMake build system (Windows/MSVC)
- [x] World chunk binary format parsing (.chunk_pc / .g_chunk_pc)
- [x] Chunk texture name extraction
- [x] Chunk GPU geometry decoding with multi-submesh support
- [x] Streaming manager with real chunk bounds from headers
- [x] Chunk analyzer CLI tool
- [x] Chunk viewer (SDL2+Vulkan, FPS camera)
- [x] Asset manager VFS integration (PEG + mesh from-memory loading)
- [x] Vulkan texture binding with descriptor sets and compiled SPIR-V shaders
- [x] Character mesh and static mesh from-memory parsing
- [x] Animation keyframe parsing (Full14 + CompactRot6 compressed formats)
- [x] Full VPP virtual filesystem mounting
- [x] Preload table parsing
- [x] XTBL configuration parsing

### Planned
- [ ] Entity-component system
- [ ] Audio (OpenAL)
- [ ] UI system
- [ ] Physics
- [ ] Scripting
- [ ] AI pathfinding
- [ ] Mission system

## Building

### Requirements
- CMake 3.16+
- C++20 compiler (MSVC 2019+, GCC 10+, Clang 10+)
- Optional: SDL2, Vulkan SDK, GLM, OpenAL Soft

### Quick Start

```bash
# Clone the repository
git clone https://github.com/sp00nznet/OpenSaints.git
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

### Asset Testing Tool
```bash
# List contents of a VPP archive
asset_test list-vpp common.vpp_pc

# Extract VPP to directory
asset_test extract-vpp common.vpp_pc ./extracted

# List textures in a PEG package
asset_test list-peg aisha.peg_pc

# Extract texture to BMP (viewable in Windows)
asset_test extract-tex aisha.peg_pc "cmcoa_waisha01_d.tga" output.bmp

# Extract texture to TGA
asset_test extract-tex aisha.peg_pc "pat_zebra01.tga" output.tga

# Find all PEG files in directory
asset_test find-pegs ./extracted

# Scan game directory for assets
asset_test scan-game "C:/Games/Saints Row 2"
```

### Chunk Analyzer
```bash
# Display parsed header fields
chunk_analyzer header sr2_chunk007.chunk_pc

# List texture names embedded in chunk
chunk_analyzer textures sr2_chunk006.chunk_pc

# Analyze GPU vertex/index buffer layout
chunk_analyzer geometry sr2_chunk012.chunk_pc

# Compare all chunks in a directory (signature, bounds, GPU sizes)
chunk_analyzer compare ./temp_chunk_extract

# Hex dump of key header regions
chunk_analyzer hexdump sr2_chunk007.chunk_pc
```

### Chunk Viewer (requires renderer)
```bash
# Browse and render extracted chunk geometry with FPS camera
chunk_viewer ./temp_chunk_extract
```

## Project Structure

```
OpenSaints/
├── src/
│   ├── formats/          # File format parsers
│   │   ├── vpp.*         # VPP archive format
│   │   ├── peg.*         # Texture packages
│   │   ├── mesh.*        # Character/static meshes
│   │   ├── chunk.*       # World chunks (.chunk_pc/.g_chunk_pc)
│   │   ├── anim.*        # Animation clips
│   │   ├── xtbl.*        # XML config tables
│   │   └── preload_table.*
│   ├── engine/           # Core engine systems
│   │   ├── vfs.*         # Virtual filesystem
│   │   ├── asset_manager.* # Asset loading/caching
│   │   ├── entity.*      # Entity-component system
│   │   └── animation.*   # Animation controller
│   ├── world/            # World management
│   │   └── streaming.*   # Chunk streaming (real bounds + VFS loading)
│   ├── render/           # Vulkan rendering
│   │   ├── renderer.*    # Abstract renderer
│   │   └── vulkan_backend.*
│   ├── audio/            # OpenAL audio
│   │   └── audio_system.*
│   ├── ui/               # UI system
│   │   └── vint_doc.*    # VINT document parser
│   ├── script/           # Scripting
│   │   └── script_system.* # Lua + action nodes
│   ├── physics/          # Physics simulation
│   │   └── physics_system.*
│   ├── platform/         # Platform abstraction
│   │   └── application.* # SDL2 window/input
│   ├── chunk_analyzer.cpp # CLI tool for chunk format analysis
│   ├── chunk_viewer.cpp  # SDL2+Vulkan chunk geometry viewer
│   └── asset_viewer.cpp  # SDL2+Vulkan mesh/texture viewer
├── tools/                # Utility tools
│   ├── vpp_extract.*     # VPP extraction
│   ├── extract_all.py    # Batch extraction
│   └── pe_analyze.py     # Executable analysis
└── docs/                 # Documentation
```

## Documentation

- [Development Status](docs/development-status.md) - Current progress and next steps
- [Vulkan Renderer](docs/vulkan-renderer.md) - Implementation notes and issues solved
- [Architecture Overview](docs/architecture.md) - System design and streaming pipeline
- [File Formats Reference](docs/formats.md) - SR2 format documentation (VPP, PEG, Chunk verified)

## Technical Details

### Supported Formats

| Extension | Type | Status |
|-----------|------|--------|
| `.vpp_pc` | Archive | **Working** - extraction and listing |
| `.peg_pc`/`.g_peg_pc` | Textures | **Working** - parse, DXT decode, export |
| `.cpeg_pc`/`.gpeg_pc` | Textures | **Working** - parse, DXT decode, export |
| `.cvbm_pc`/`.gvbm_pc` | Textures | **Working** - parse, DXT decode, export |
| `.cmesh_pc`/`.gcmesh_pc` | Character mesh | **Working** - disk + memory, skinned vertices |
| `.smesh_pc`/`.gsmesh_pc` | Static mesh | **Working** - disk + memory parsing |
| `.chunk_pc`/`.g_chunk_pc` | World geometry | **Working** - header, bounds, textures, multi-submesh GPU decode |
| `.xtbl` | Config tables | **Working** - full DOM-style XML parser |
| `.anim_pc` | Animations | **Working** - keyframe parsing, bone tracks |
| `.vint_doc` | UI documents | Skeleton code |

### Engine Architecture

OpenSaints uses a modular architecture:

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  (Demo, Game Logic, Mission System)                     │
├─────────────────────────────────────────────────────────┤
│                     Systems Layer                        │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │
│  │  UI     │ │ Script  │ │ Physics │ │  Audio  │       │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘       │
├─────────────────────────────────────────────────────────┤
│                     Engine Layer                         │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │
│  │ Entity  │ │Animation│ │Streaming│ │ Render  │       │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘       │
├─────────────────────────────────────────────────────────┤
│                    Platform Layer                        │
│  ┌─────────────────┐ ┌─────────────────┐               │
│  │  Asset Manager  │ │ Virtual FS (VFS)│               │
│  └─────────────────┘ └─────────────────┘               │
├─────────────────────────────────────────────────────────┤
│                    Format Parsers                        │
│  VPP | PEG | Mesh | Chunk | Anim | XTBL | Preload      │
└─────────────────────────────────────────────────────────┘
```

### Key Systems

- **Virtual Filesystem**: Mounts VPP archives with priority support for patches
- **Asset Manager**: Reference-counted loading with 512MB default memory budget
- **Streaming**: Priority-based chunk loading with predictive pre-loading
- **Animation**: Layered blending with state machine support
- **Physics**: Fixed-timestep simulation with collision detection
- **Audio**: 3D spatial sound with channel-based volume mixing

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
- Cross-platform testing
- AI pathfinding implementation
- Documentation

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- The Saints Row modding community for format documentation
- Volition for creating Saints Row 2
- Contributors to open-source game reimplementation projects

---

*OpenSaints is not affiliated with Volition, Deep Silver, or any other rights holders of Saints Row.*
