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

**Early Development** - Core rendering works, asset loading in progress.

See [docs/development-status.md](docs/development-status.md) for detailed status.

### Working
- [x] Vulkan rendering backend with basic geometry
- [x] SDL2 window/input handling
- [x] VPP archive extraction (Python and C++ tools)
- [x] Demo scene with rotating objects
- [x] CMake build system (Windows/MSVC)

### In Progress
- [ ] Asset manager integration with VFS
- [ ] PEG texture loading and rendering
- [ ] Mesh loading and rendering

### Planned (Phase 1: Asset Pipeline)
- [ ] Full VPP virtual filesystem mounting
- [ ] Preload table parsing
- [ ] XTBL configuration parsing
- [ ] World chunk loading
- [ ] Animation loading

### Planned (Phase 2+)
- [ ] World streaming
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

## Project Structure

```
OpenSaints/
├── src/
│   ├── formats/          # File format parsers
│   │   ├── vpp.*         # VPP archive format
│   │   ├── peg.*         # Texture packages
│   │   ├── mesh.*        # Character/static meshes
│   │   ├── chunk.*       # World chunks
│   │   ├── anim.*        # Animation clips
│   │   ├── xtbl.*        # XML config tables
│   │   └── preload_table.*
│   ├── engine/           # Core engine systems
│   │   ├── vfs.*         # Virtual filesystem
│   │   ├── asset_manager.* # Asset loading/caching
│   │   ├── entity.*      # Entity-component system
│   │   └── animation.*   # Animation controller
│   ├── world/            # World management
│   │   └── streaming.*   # Chunk streaming
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
│   └── platform/         # Platform abstraction
│       └── application.* # SDL2 window/input
├── tools/                # Utility tools
│   ├── vpp_extract.*     # VPP extraction
│   ├── extract_all.py    # Batch extraction
│   └── pe_analyze.py     # Executable analysis
└── docs/                 # Documentation
```

## Documentation

- [Development Status](docs/development-status.md) - Current progress and next steps
- [Vulkan Renderer](docs/vulkan-renderer.md) - Implementation notes and issues solved
- [Architecture Overview](docs/architecture.md) - System design (planned)
- [File Formats Reference](docs/formats.md) - SR2 format documentation (planned)

## Technical Details

### Supported Formats

| Extension | Type | Status |
|-----------|------|--------|
| `.vpp_pc` | Archive | Full support |
| `.cpeg_pc`/`.gpeg_pc` | Textures | Full support |
| `.cvbm_pc`/`.gvbm_pc` | Textures | Full support |
| `.cmesh_pc`/`.gcmesh_pc` | Character mesh | Basic support |
| `.smesh_pc`/`.gsmesh_pc` | Static mesh | Basic support |
| `.chunk_pc` | World geometry | Basic support |
| `.xtbl` | Config tables | Full support |
| `.anim_pc` | Animations | Basic support |
| `.vint_doc` | UI documents | Basic support |

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
