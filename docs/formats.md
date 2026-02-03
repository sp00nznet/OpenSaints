# Saints Row 2 File Formats

This document describes the file formats used by Saints Row 2 and how OpenSaints parses them.

## Archive Format (VPP_PC)

VPP (Volition Package) archives contain all game assets.

### Header Structure

```
Offset  Size    Description
0x000   4       Magic: 0x51890ACE
0x004   4       Version: 4
0x008   0x14C   Padding/Unknown
0x154   4       Number of files (signed int32)
0x158   4       Container size
0x15C   4       Length of offset table
0x160   4       Length of filename section
0x164   4       Length of extension section
0x168   0x24    Reserved
```

### Section Layout

```
┌────────────────────────┐ 0x000
│       Header           │
├────────────────────────┤ 0x800 (aligned)
│    File Entry Table    │
├────────────────────────┤ (aligned to 0x800)
│   Filename Strings     │
├────────────────────────┤ (aligned to 0x800)
│   Extension Strings    │
├────────────────────────┤ (aligned to 0x800)
│      File Data         │
└────────────────────────┘
```

### File Entry (28 bytes)

```
Offset  Size    Description
0x00    4       Filename offset (into filename section)
0x04    4       Extension offset (into extension section)
0x08    4       Unknown
0x0C    4       Data offset (from data section start)
0x10    4       Data size
0x14    4       Always -1
0x18    4       Always 0
```

### Known VPP Archives

| Archive | Contents |
|---------|----------|
| `common.vpp_pc` | Common assets (XTBL, shaders, etc.) |
| `meshes.vpp_pc` | Character and object meshes |
| `textures.vpp_pc` | Texture packages |
| `chunks1-4.vpp_pc` | World geometry by region |
| `anims.vpp_pc` | Character animations |
| `audio.vpp_pc` | Sound effects |
| `music1-4.vpp_pc` | Music tracks |
| `patch.vpp_pc` | Post-release patches |

---

## Texture Package (PEG)

PEG (Packed Exact Geometry) files store texture data. They come in CPU/GPU pairs:
- `.cpeg_pc` / `.cvbm_pc` - CPU header file
- `.gpeg_pc` / `.gvbm_pc` - GPU data file

### CPU Header

```
Offset  Size    Description
0x00    4       Signature: 0x564B4547 ("GEKV")
0x04    2       Version: 10
0x06    2       Platform (0 = PC)
0x08    4       Header size
0x0C    4       Total data size
0x10    2       Number of textures
0x12    2       Flags
0x14    2       Total frames
0x16    2       Reserved
```

### Texture Entry (32 bytes)

```
Offset  Size    Description
0x00    4       Data offset in GPU file
0x04    2       Width
0x06    2       Height
0x08    2       Format (D3DFORMAT)
0x0A    2       Flags
0x0C    2       Name offset
0x0E    2       Source width
0x10    2       Source height
0x12    1       Mip levels
0x13    1       Frames
0x14    2       Frame delay
0x16    4       Data size
0x1A    8       Reserved
```

### Texture Formats

| Value | Format | Description |
|-------|--------|-------------|
| 400 | DXT1 | BC1, 4bpp, 1-bit alpha |
| 401 | DXT3 | BC2, 8bpp, explicit alpha |
| 402 | DXT5 | BC3, 8bpp, interpolated alpha |
| 565 | R5G6B5 | 16-bit RGB |
| 1555 | A1R5G5B5 | 16-bit ARGB |
| 8888 | A8R8G8B8 | 32-bit ARGB |
| 28 | A8 | 8-bit alpha only |

---

## Mesh Formats

Meshes also use CPU/GPU pairs:
- `.cmesh_pc` / `.gcmesh_pc` - Character meshes (skinned)
- `.smesh_pc` / `.gsmesh_pc` - Static meshes

### General Structure

```
CPU File:
┌────────────────────────┐
│       Header           │
├────────────────────────┤
│   Submesh Info         │
├────────────────────────┤
│   Material Refs        │
├────────────────────────┤
│   Bone Data (cmesh)    │
└────────────────────────┘

GPU File:
┌────────────────────────┐
│    Vertex Buffer       │
├────────────────────────┤
│    Index Buffer        │
└────────────────────────┘
```

### Vertex Formats

**Static Mesh Vertex (32 bytes typical):**
```
Offset  Size    Description
0x00    12      Position (float3)
0x0C    12      Normal (float3)
0x18    8       UV (float2)
```

**Character Mesh Vertex (40 bytes typical):**
```
Offset  Size    Description
0x00    12      Position (float3)
0x0C    12      Normal (float3)
0x18    8       UV (float2)
0x20    4       Bone indices (ubyte4)
0x24    4       Bone weights (ubyte4, normalized)
```

---

## World Chunks

`.chunk_pc` files contain world geometry for streaming.

### Header

```
Offset  Size    Description
0x00    4       Signature
0x04    4       Version
0x08    4       Flags
0x0C    4       Object count
0x10    12      Bounding box min (float3)
0x1C    12      Bounding box max (float3)
0x28    12      Origin (float3)
0x34    4       Zone ID
0x38    4       LOD level
0x3C    4       Data offset
0x40    4       Data size
```

### Object Types

| Type | Description |
|------|-------------|
| 1 | Terrain |
| 2 | Building |
| 3 | Prop |
| 4 | Collision |
| 5 | Decal |
| 6 | Light |
| 7 | Particle emitter |
| 8 | Audio source |
| 9 | Navigation mesh |

---

## Configuration Tables (XTBL)

XTBL files are XML-based configuration tables.

### Structure

```xml
<?xml version="1.0" encoding="utf-8"?>
<root>
    <Table>
        <Entry>
            <Name>entry_name</Name>
            <Property1>value1</Property1>
            <Property2>value2</Property2>
        </Entry>
        <!-- More entries -->
    </Table>
</root>
```

### Common XTBL Files

| File | Contents |
|------|----------|
| `achievements.xtbl` | Achievement definitions |
| `action_nodes.xtbl` | Animation action nodes |
| `vehicles.xtbl` | Vehicle definitions |
| `weapons.xtbl` | Weapon stats |
| `npcs.xtbl` | NPC definitions |
| `missions.xtbl` | Mission data |
| `radio_stations.xtbl` | Radio station config |

---

## Preload Tables

Text-based files defining asset load order.

### Format

```
# Comment
[Category]
1 "filename1.ext"
2 "filename2.ext"
```

Or simpler:
```
1 filename1.ext
2 filename2.ext
```

### Files

- `preload.tbl` - General asset preloading
- `preload_anim.tbl` - Animation preloading

---

## Animation Format (Planned)

`.anim_pc` files contain skeletal animation data.

### Expected Structure

```
Header:
- Bone count
- Frame count
- Duration
- Flags

Per-bone data:
- Position keyframes
- Rotation keyframes (quaternion)
- Scale keyframes
```

---

## Audio Formats (Planned)

Saints Row 2 uses custom audio formats based on:
- XMA for Xbox 360
- Platform-specific codecs for PC

The PC version likely uses:
- `.xwb` - XACT Wave Banks
- `.xsb` - XACT Sound Banks

---

## Shader Formats

### Compiled Shaders

- `.pso_pc` - Pixel shaders (compiled D3D9 bytecode)
- `.vso_pc` - Vertex shaders (compiled D3D9 bytecode)

### Shader Package

The `shaders/` directory contains pre-compiled shaders organized by technique.

---

## UI Documents (Planned)

`.vint_doc` files define UI layouts using Volition's internal format.

### Expected Structure

- Widget hierarchy
- Position/size data
- Event bindings
- Animation data

---

## References

- Kaitai Struct VPP specification
- Community format documentation
- TCRF Saints Row 2 analysis

*Note: Some format details are still being reverse-engineered. Contributions welcome!*
