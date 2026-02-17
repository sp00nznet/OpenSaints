# OpenSaints Architecture

This document describes the high-level architecture of the OpenSaints engine.

## Overview

OpenSaints follows a layered architecture with clear separation between:

1. **Format Layer** - Parsers for game file formats
2. **Engine Layer** - Core systems (VFS, asset management, etc.)
3. **Subsystem Layer** - Rendering, audio, physics, etc.
4. **Game Layer** - Game logic, entities, world

```
┌─────────────────────────────────────────────────────────┐
│                      Game Layer                         │
│   (Missions, AI, Vehicles, Player, UI)                 │
├─────────────────────────────────────────────────────────┤
│                   Subsystem Layer                       │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│   │ Renderer │ │  Audio   │ │ Physics  │ │  Input   │  │
│   └──────────┘ └──────────┘ └──────────┘ └──────────┘  │
├─────────────────────────────────────────────────────────┤
│                    Engine Layer                         │
│   ┌──────────────────┐ ┌──────────────────────────────┐│
│   │ Asset Manager    │ │ World/Zone Manager           ││
│   ├──────────────────┤ ├──────────────────────────────┤│
│   │ Virtual FS (VFS) │ │ Entity System                ││
│   └──────────────────┘ └──────────────────────────────┘│
├─────────────────────────────────────────────────────────┤
│                    Format Layer                         │
│   ┌─────┐ ┌─────┐ ┌──────┐ ┌──────┐ ┌───────┐ ┌─────┐ │
│   │ VPP │ │ PEG │ │ Mesh │ │Chunk │ │ XTBL  │ │Anim │ │
│   └─────┘ └─────┘ └──────┘ └──────┘ └───────┘ └─────┘ │
└─────────────────────────────────────────────────────────┘
```

## Component Details

### Format Layer

Located in `src/formats/`, these are pure parsers with no dependencies on engine systems.

| Component | Files | Purpose |
|-----------|-------|---------|
| VPP | `vpp.h/cpp` | Archive extraction |
| PEG | `peg.h/cpp` | Texture packages |
| Mesh | `mesh.h/cpp` | 3D geometry |
| Chunk | `chunk.h/cpp` | World geometry (CPU header + GPU vertex/index data) |
| XTBL | `xtbl.h/cpp` | Configuration data |
| Preload | `preload_table.h/cpp` | Asset dependencies |

### Engine Layer

Located in `src/engine/`, these provide core services.

#### Virtual Filesystem (VFS)

The VFS provides a unified view of game assets:

```cpp
VirtualFileSystem vfs;
vfs.mountDirectory("path/to/game", "/", 0);

// Access any file from any VPP archive
auto data = vfs.read("player_mesh.cmesh_pc");
```

Features:
- Mount multiple VPP archives
- Priority-based file resolution (patches override base files)
- Case-insensitive path lookup
- File pattern matching

#### Asset Manager

Handles asset lifecycle:

```cpp
AssetManager assets;
assets.initialize(vfs);

// Load with automatic caching
auto texture = assets.loadTexture("player_diffuse");
auto mesh = assets.loadMesh("player");

// Async loading
auto future = assets.loadTextureAsync("large_texture");
```

Features:
- Reference-counted handles
- Memory budget management
- Lazy loading
- Async loading support
- Hot reloading (development)

### Subsystem Layer

#### Renderer (Planned)

Vulkan-based renderer with:
- Mesh rendering
- Texture management
- Shader system (translate or replace original shaders)
- Post-processing

#### Audio (Planned)

OpenAL-based audio with:
- 3D positional audio
- Streaming music
- Sound effects

#### Physics (Planned)

Physics integration for:
- Collision detection
- Vehicle physics
- Ragdoll

### Game Layer

#### World Manager / Streaming

Handles streaming of world chunks based on player position:

```cpp
StreamingManager streaming;
streaming.initialize(assetManager);
streaming.discoverChunks();  // Quick-parses headers for real bounds

// Each frame:
streaming.update(playerPos, playerVelocity, deltaTime);

// Get visible chunks for rendering
auto visible = streaming.getVisibleChunks();
for (auto& chunk : visible) {
    // chunk->data().meshes contains decoded geometry
    // chunk->data().textures contains texture name references
}
```

Chunk loading pipeline:
1. `discoverChunks()` reads headers from VFS, extracts bounds at 0xD4
2. `update()` queues chunks within load radius based on distance/priority
3. `loadChunkSync()` reads CPU+GPU files, calls `WorldChunk::openFromMemory()`
4. GPU vertex/index buffers are decoded with auto-stride detection
5. Distant chunks are evicted via LRU when over memory budget

#### Entity System

Component-based entity management:

```cpp
Entity* player = entityManager.create("Player");
player->addComponent<Transform>();
player->addComponent<MeshRenderer>();
player->addComponent<PlayerController>();
```

## Data Flow

### Asset Loading

```
User Request
    │
    ▼
Asset Manager (check cache)
    │
    ▼ (cache miss)
Virtual Filesystem (find file)
    │
    ▼
VPP Archive (extract raw data)
    │
    ▼
Format Parser (decode to engine format)
    │
    ▼
Asset Manager (cache result)
    │
    ▼
Return Handle to User
```

### World Streaming

```
Player Position Update
    │
    ▼
Zone Manager (determine active zones)
    │
    ▼
Chunk Loader (queue chunk loads/unloads)
    │
    ▼
Asset Manager (load chunk assets)
    │
    ▼
Scene Graph (add/remove geometry)
    │
    ▼
Renderer (draw visible chunks)
```

## Threading Model

```
Main Thread          Asset Thread(s)       Render Thread
    │                     │                     │
    │  ┌──────────────────┼─────────────────────┤
    │  │ Game Logic       │ Async Loading       │ GPU Submit
    │  │ Input            │ Decompression       │ Present
    │  │ Physics          │ Parsing             │
    │  │ Audio            │                     │
    │  └──────────────────┼─────────────────────┤
    ▼                     ▼                     ▼
```

## Memory Management

### Budget System

```cpp
assets.setMemoryBudget(512 * 1024 * 1024); // 512 MB

// Automatic trimming when over budget
assets.trimToMemoryBudget();

// Manual garbage collection
assets.collectGarbage();
```

### Reference Counting

Assets are automatically released when no longer referenced:

```cpp
{
    auto texture = assets.loadTexture("temp");
    // refCount = 1
} // refCount = 0, eligible for garbage collection
```

## Configuration

Game configuration is loaded from XTBL files:

```cpp
auto vehicles = assets.loadXTable("vehicles");
for (auto* entry : vehicles->document()->entries()) {
    Vehicle v = xtbl::Vehicle::fromNode(entry);
    registerVehicle(v);
}
```

## Error Handling

The engine uses a combination of:
- Return values for expected failures
- Exceptions for programmer errors
- Logging for diagnostics

```cpp
// Expected failure - returns empty/null
auto handle = assets.loadTexture("nonexistent");
if (!handle) {
    // Use fallback texture
}

// Diagnostic logging
std::cerr << "Warning: Texture not found\n";
```

## Future Considerations

### Modding Support

The architecture is designed to support mods:
- VFS priority system allows mods to override base files
- XTBL parser supports additional entries
- Asset manager can load from external directories

### Network Play

The entity system and world manager are designed with potential multiplayer in mind:
- Deterministic entity IDs
- Separable game state
- Position interpolation support
