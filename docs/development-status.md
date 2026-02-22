# OpenSaints Development Status

Last updated: February 2026

## Current State

OpenSaints is in early development. The core rendering pipeline is functional with basic geometry rendering. Asset loading from the actual game files is the next priority.

### What Works

| Component | Status | Notes |
|-----------|--------|-------|
| **Vulkan Renderer** | Working | Geometry, MVP, vertex colors, texture binding with descriptor sets |
| **SDL2 Window** | Working | Window creation, input handling, mouse capture |
| **Demo Scene** | Working | Rotating triangle and cube |
| **VPP Extraction** | Working | Python and C++ tools |
| **Build System** | Working | CMake, builds on Windows with MSVC |
| **Asset Manager** | Working | Full VFS integration, PEG + mesh from-memory loading |
| **VFS** | Working | Mount VPPs, priority resolution, file read |
| **Mesh Parser** | Working | Character + static mesh from disk and memory |
| **Texture Shaders** | Working | SPIR-V with texture sampling + diffuse/ambient lighting |
| **Multi-submesh Chunks** | Working | Per-render-item submesh splitting with material indices |
| **Animation Parser** | Working | Keyframe parsing (Full14 + CompactRot6 formats) |
| **Preload Tables** | Working | Text-based ID-to-filename mapping |
| **XTBL Parser** | Working | Full DOM-style XML parser |

### What Works (Asset Loading)

| Component | Status | Notes |
|-----------|--------|-------|
| **VPP Parser** | Working | Extracts all files from .vpp_pc archives |
| **PEG Parser** | Working | Parses .peg_pc/.g_peg_pc texture packages |
| **DXT Decoder** | Working | DXT1/DXT3/DXT5 to RGBA conversion |
| **Texture Export** | Working | BMP and TGA output formats |
| **asset_test CLI** | Working | Tool for inspecting/extracting assets |
| **Chunk Header** | Working | Real binary format: signature 0xBBCACA12, version 121 |
| **Chunk Bounds** | Working | World-space bounding boxes at offset 0xD4 |
| **Chunk Textures** | Working | Null-terminated texture name extraction |
| **Chunk GPU Decode** | Working | Vertex + index buffer parsing from .g_chunk_pc |
| **Chunk Analyzer** | Working | CLI tool with header/textures/geometry/compare/hexdump |
| **Chunk Viewer** | Working | SDL2+Vulkan FPS camera viewer for chunk geometry |
| **Streaming Manager** | Working | Real bounds from headers, VFS-based chunk loading |

### Not Started

| Component | Notes |
|-----------|-------|
| Animation playback | Parser complete, needs runtime integration |
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
│   ├── asset_viewer.cpp      # 3D mesh/texture viewer (WORKING)
│   ├── chunk_analyzer.cpp    # Chunk format analysis CLI (WORKING)
│   ├── chunk_viewer.cpp      # Chunk geometry viewer (WORKING)
│   ├── formats/              # File parsers
│   │   ├── chunk.h/cpp       # World chunks (WORKING - real binary format)
│   │   ├── vpp.h/cpp         # VPP archives (WORKING)
│   │   ├── peg.h/cpp         # Textures (WORKING - disk + memory)
│   │   ├── mesh.h/cpp        # Meshes (WORKING - disk + memory)
│   │   └── anim.h/cpp        # Animations (WORKING - keyframe parsing)
│   ├── engine/               # Core systems (WORKING)
│   ├── world/
│   │   └── streaming.h/cpp   # Chunk streaming (WORKING - real bounds)
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

### Immediate Priority: End-to-End Textured Rendering

1. **Textured chunk rendering** - Wire up PEG textures to chunk submesh materials in the viewer
2. **Entity system** - Basic entity spawning with mesh + transform components
3. **Render queue** - Batched draw calls sorted by material/texture

### Subsequent Priorities

1. Animation playback integration with skinned meshes
2. Audio system integration (OpenAL)
3. UI system (VINT document rendering)
4. Physics integration
5. Scripting system

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

### Session: February 16, 2026

**Goal**: Implement world chunk loading (reverse-engineer .chunk_pc binary format)

**Accomplished**:
- Reverse-engineered .chunk_pc binary format from hex analysis of extracted chunks
- Created chunk_analyzer CLI tool (header/textures/geometry/compare/hexdump subcommands)
- Replaced incorrect ChunkHeader struct with real ChunkFileHeader (0x8C) + ChunkGeometryInfo (0x64)
- Implemented texture name parsing (count at 0x100, null-terminated strings with variable padding)
- Implemented GPU geometry decoding from .g_chunk_pc files (VB + IB split based on header sizes)
- Auto-stride detection (tests 20/24/28/32/36/40 against local-space half-extents)
- Created chunk_viewer SDL2+Vulkan tool with FPS camera for visual chunk inspection
- Fixed StreamingManager to read real bounds from chunk headers and load via VFS
- Updated docs/formats.md with verified binary format documentation

**Chunk Format Findings**:
- `.chunk_pc` = CPU file, `.g_chunk_pc` = GPU data (same CPU/GPU pattern as meshes and textures)
- Signature: 0xBBCACA12, version 121, flags 14 (constant across all 100+ chunks)
- GPU sizes at 0x8C (VB) and 0x90 (IB) always sum to .g_chunk_pc file size exactly
- Bounding box at 0xD4 (min) and 0xE0 (max) in world-space coordinates
- Texture names at 0x100+ as null-terminated ASCII strings (e.g. "kt_stoneStrata02_co.tga")
- Vertex format: float3 position (LOCAL SPACE) + uint8x4 packed normal + uint16x2 UV
- Vertex positions are relative to bounds center, NOT world-space
- Multi-submesh chunks have complex interleaved VB data requiring render item descriptor parsing
