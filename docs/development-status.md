# OpenSaints Development Status

Last updated: February 2026

## Current State

OpenSaints is in early development. The core rendering pipeline is functional with basic geometry rendering. Asset loading from the actual game files is the next priority.

### What Works

| Component | Status | Notes |
|-----------|--------|-------|
| **Vulkan Renderer** | Working | Basic geometry, MVP transforms, vertex colors |
| **SDL2 Window** | Working | Window creation, input handling, mouse capture |
| **Demo Scene** | Working | Rotating triangle and cube |
| **VPP Extraction** | Working | Python tool extracts archives |
| **Build System** | Working | CMake, builds on Windows with MSVC |

### In Progress

| Component | Status | Notes |
|-----------|--------|-------|
| **Asset Manager** | Skeleton | Code exists, not tested with real assets |
| **VFS** | Skeleton | Code exists, not integrated |
| **Mesh Parser** | Skeleton | Code exists, not tested |
| **Texture Rendering** | Not started | Shaders only use vertex colors currently |

### What Works (Asset Loading)

| Component | Status | Notes |
|-----------|--------|-------|
| **VPP Parser** | Working | Extracts all files from .vpp_pc archives |
| **PEG Parser** | Working | Parses .peg_pc/.g_peg_pc texture packages |
| **DXT Decoder** | Working | DXT1/DXT3/DXT5 to RGBA conversion |
| **Texture Export** | Working | BMP and TGA output formats |
| **asset_test CLI** | Working | Tool for inspecting/extracting assets |

### Not Started

| Component | Notes |
|-----------|-------|
| World/chunk loading | Parser exists but not integrated |
| Animation playback | Parser exists but not integrated |
| Audio | OpenAL skeleton exists |
| UI | VINT parser skeleton exists |
| Physics | Skeleton exists |
| Scripting | Skeleton exists |

## Build Requirements

### Tested Configuration
- Windows 11
- MSVC 2022 (Visual Studio 17.x)
- CMake 3.16+
- Vulkan SDK 1.4.335.0 (for shader compilation)

### Runtime Dependencies
- SDL2 2.28.5 (bundled in external/)
- Vulkan runtime (installed with GPU drivers)
- volk (bundled, loads Vulkan dynamically)
- GLM (bundled in external/)

## Directory Structure (Actual)

```
OpenSaints/
├── src/
│   ├── demo.cpp              # Demo application (WORKING)
│   ├── main.cpp              # Main application entry
│   ├── formats/              # File parsers (SKELETON)
│   ├── engine/               # Core systems (SKELETON)
│   ├── render/
│   │   ├── renderer.h/cpp    # Abstract interface
│   │   ├── vulkan_backend.*  # Vulkan impl (WORKING)
│   │   ├── default_shaders.h # Embedded SPIR-V
│   │   └── shaders/          # GLSL source
│   ├── platform/
│   │   └── application.*     # SDL2 window (WORKING)
│   └── [other subsystems]    # SKELETON
├── tools/
│   ├── vpp_extract.py        # Python VPP tool (WORKING)
│   └── vpp_extract.cpp       # C++ VPP tool (WORKING)
├── external/                 # Dependencies (not in git)
│   ├── SDL2-2.28.5/
│   ├── glm/
│   ├── vulkan/               # Headers only
│   ├── volk.h/c
│   └── glslc.exe             # Shader compiler
├── build/                    # CMake build output
└── docs/                     # Documentation
```

## Next Steps

### Immediate Priority: Asset Loading

1. **Test VPP extraction** with actual Saints Row 2 installation
2. **Implement asset manager integration** with VFS
3. **Test PEG texture parser** - load and display a texture
4. **Test mesh parser** - load and render a static mesh

### Subsequent Priorities

1. World chunk loading and rendering
2. Character mesh + animation
3. Basic lighting in shaders
4. Audio system integration

## Known Issues

1. **Per-object transforms**: Currently all objects share one uniform buffer, causing race conditions. Workaround: bake positions into vertex data.

2. **Shader compilation**: Requires Vulkan SDK installed for `glslc`. Pre-compiled SPIR-V is embedded for runtime.

3. **Windows-only**: Not tested on Linux/macOS yet. Should work with minor CMake adjustments.

4. **No validation layers**: Debug builds should enable Vulkan validation for development.

## Session Log

### Session: February 4, 2026

**Goal**: Get Vulkan renderer displaying geometry

**Accomplished**:
- Fixed SDL_main linker error on Windows
- Fixed swapchain format mismatch causing black screen
- Fixed VkClearValue union initialization
- Implemented descriptor set allocation and binding
- Fixed triangle winding order for correct culling
- Fixed critical bug: bound state not reset between frames
- Created demo with rotating triangle and cube

**Time spent**: ~3 hours debugging Vulkan initialization and rendering

**Key learnings**:
- Always reset bound state tracking when command buffer is reset
- VkClearValue union needs explicit member assignment
- Store actual swapchain format, don't assume
- Descriptor sets must be allocated AND bound

### Session: February 4-5, 2026 (Continued)

**Goal**: Asset loading from Saints Row 2 game files

**Accomplished**:
- Extracted 6166 files from common.vpp_pc
- Implemented PEG texture parser with proper struct layout (48-byte entries)
- Fixed name table parsing (sequential names, not offset-based)
- Implemented DXT1/DXT3/DXT5 decompression to RGBA
- Added BMP and TGA export to asset_test CLI
- Verified extraction with aisha.peg_pc (character textures)

**PEG Format Findings**:
- `.peg_pc` = CPU header file, `.g_peg_pc` = GPU data file
- Header: 20 bytes (GEKV signature, version 10, texture count)
- Entries: 48 bytes each (not 32 as initially assumed)
- Name table: Sequential null-terminated strings (entry index matches name index)
- Format codes: 400=DXT1, 401=DXT3, 402=DXT5
- Data sizes in struct are unreliable; calculate from dimensions

**Pattern Texture Note**:
SR2's clothing pattern textures (stripes, zebra, plaid) appear as blue/cyan colors because they are mask textures combined with player-selected colors at runtime.
