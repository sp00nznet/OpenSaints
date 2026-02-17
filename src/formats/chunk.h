#pragma once
// Chunk parser for Saints Row 2
// Handles .chunk_pc / .g_chunk_pc files containing world/level geometry
// Chunks are streaming units for the open world
//
// Binary format (reverse-engineered):
//   .chunk_pc  = CPU header: signature, bounds, texture names, render item descriptors
//   .g_chunk_pc = GPU data:  vertex buffer + index buffer (uint16) back-to-back

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>

namespace opensaints {

// Forward declarations
struct MeshData;

// Chunk file signature and version
constexpr uint32_t CHUNK_SIGNATURE = 0xBBCACA12;
constexpr uint32_t CHUNK_VERSION = 121;

#pragma pack(push, 1)

// Chunk file header (0x00 - 0x87, 136 bytes)
// Replaces the old incorrect ChunkHeader
struct ChunkFileHeader {
    uint32_t signature;          // 0x00: 0xBBCACA12
    uint32_t version;            // 0x04: 121
    uint32_t flags;              // 0x08: typically 14
    uint32_t unknown_0C;         // 0x0C: always 0
    uint32_t section_count;      // 0x10: number of sections/render items
    uint8_t  unknown_14[0x78];   // 0x14 - 0x8B: various fields (hashes, pointers, etc.)
};
static_assert(sizeof(ChunkFileHeader) == 0x8C, "ChunkFileHeader must be 0x8C bytes");

// Chunk geometry info (0x8C - 0xEF, 100 bytes)
struct ChunkGeometryInfo {
    uint32_t gpu_vertex_size;    // 0x8C: vertex buffer size in bytes
    uint32_t gpu_index_size;     // 0x90: index buffer size in bytes (0 if no indices)
    uint32_t submesh_count;      // 0x94: number of submeshes/material groups
    uint32_t unknown_98;         // 0x98
    uint32_t render_item_count;  // 0x9C: number of render items
    uint32_t unknown_A0;         // 0xA0
    uint8_t  padding_A4[0x30];   // 0xA4 - 0xD3: padding/unknown
    float    bounds_min[3];      // 0xD4: world-space bounding box min
    float    bounds_max[3];      // 0xE0: world-space bounding box max
    uint8_t  unknown_EC[4];      // 0xEC - 0xEF
};
static_assert(sizeof(ChunkGeometryInfo) == 0x64, "ChunkGeometryInfo must be 0x64 bytes");

#pragma pack(pop)

// A single render item / submesh within a chunk
struct ChunkRenderItem {
    uint32_t vertex_offset = 0;   // Byte offset into vertex buffer
    uint32_t vertex_count = 0;    // Number of vertices
    uint32_t index_offset = 0;    // Byte offset into index buffer (if present)
    uint32_t index_count = 0;     // Number of indices (if present)
    uint32_t vertex_stride = 20;  // Bytes per vertex (default: float3 + packed normal + uint16x2 UV)
    int32_t  material_index = -1; // Index into texture list
};

// Parsed chunk data
struct ChunkData {
    std::string name;

    // World bounds
    float bounds_min[3];
    float bounds_max[3];

    // Header info
    uint32_t version = 0;
    uint32_t flags = 0;
    uint32_t section_count = 0;

    // GPU buffer info
    uint32_t gpu_vertex_size = 0;
    uint32_t gpu_index_size = 0;

    // GPU raw data (populated when loading from memory with GPU file)
    std::vector<uint8_t> gpu_vertex_data;
    std::vector<uint8_t> gpu_index_data;

    // Render items (submesh descriptors)
    uint32_t render_item_count = 0;
    std::vector<ChunkRenderItem> render_items;

    // Material/texture references
    std::vector<std::string> textures;

    // Geometry data (decoded from GPU buffers)
    std::vector<std::shared_ptr<MeshData>> meshes;
};

// World chunk archive
class WorldChunk {
public:
    WorldChunk() = default;
    ~WorldChunk() = default;

    // Open a chunk file from disk (reads CPU header, optionally loads paired GPU file)
    bool open(const std::filesystem::path& path);

    // Open from memory buffers (for VFS/streaming use)
    bool openFromMemory(const std::string& name,
                        const std::vector<uint8_t>& cpuData,
                        const std::vector<uint8_t>& gpuData = {});

    // Close the chunk
    void close();

    // Get chunk data
    const ChunkData& data() const { return m_data; }

    // Check if open
    bool isOpen() const { return m_isOpen; }

    // Get world-space bounds
    void getBounds(float* min, float* max) const;

    // Check if point is inside chunk bounds
    bool containsPoint(float x, float y, float z) const;

    // Export chunk geometry to OBJ
    bool exportOBJ(const std::filesystem::path& path) const;

private:
    std::filesystem::path m_path;
    ChunkData m_data;
    bool m_isOpen = false;

    bool parseFromBuffer(const std::vector<uint8_t>& cpuData, const std::vector<uint8_t>& gpuData);
    bool parseHeader(const std::vector<uint8_t>& data);
    bool parseTextures(const std::vector<uint8_t>& data);
    bool parseGeometry(const std::vector<uint8_t>& gpuData);

    // Decode packed normal (4 bytes) to float3
    static void decodePackedNormal(uint32_t packed, float& nx, float& ny, float& nz);
};

// Zone manager for streaming chunks
class ZoneManager {
public:
    ZoneManager() = default;
    ~ZoneManager() = default;

    // Load zone information from zone definition files
    bool loadZoneDefinitions(const std::filesystem::path& path);

    // Get chunks that should be loaded for a world position
    std::vector<std::string> getChunksForPosition(float x, float y, float z) const;

    // Get zone ID for a world position
    uint32_t getZoneIdForPosition(float x, float y, float z) const;

    // Get zone name by ID
    std::string getZoneName(uint32_t zoneId) const;

private:
    struct ZoneInfo {
        uint32_t id;
        std::string name;
        float bounds_min[3];
        float bounds_max[3];
        std::vector<std::string> chunk_files;
    };

    std::vector<ZoneInfo> m_zones;
};

} // namespace opensaints
