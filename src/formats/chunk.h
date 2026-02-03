#pragma once
// Chunk parser for Saints Row 2
// Handles .chunk_pc files containing world/level geometry
// Chunks are streaming units for the open world

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>

namespace opensaints {

// Forward declarations
struct MeshData;

// Chunk file contains multiple geometry and object types
enum class ChunkObjectType : uint32_t {
    Unknown = 0,
    Terrain = 1,
    Building = 2,
    Prop = 3,
    Collision = 4,
    Decal = 5,
    Light = 6,
    Particle = 7,
    Audio = 8,
    Navigation = 9
};

#pragma pack(push, 1)

// Chunk file header
struct ChunkHeader {
    uint32_t signature;          // File signature
    uint32_t version;            // Format version
    uint32_t flags;              // Chunk flags
    uint32_t num_objects;        // Number of objects in chunk
    float bounds_min[3];         // World-space bounding box
    float bounds_max[3];
    float origin[3];             // Chunk origin in world space
    uint32_t zone_id;            // Zone/district this chunk belongs to
    uint32_t lod_level;          // LOD level (0 = highest detail)
    uint32_t data_offset;        // Offset to object data
    uint32_t data_size;          // Size of object data
};

// Object entry in chunk
struct ChunkObjectEntry {
    uint32_t type;               // ChunkObjectType
    uint32_t flags;              // Object flags
    float transform[16];         // 4x4 transformation matrix
    uint32_t mesh_index;         // Index into mesh array
    uint32_t material_index;     // Index into material array
    uint32_t collision_index;    // Index into collision array
    uint32_t data_offset;        // Offset to object-specific data
    uint32_t data_size;          // Size of object-specific data
};

// Light definition
struct ChunkLight {
    float position[3];
    float color[3];
    float intensity;
    float radius;
    uint32_t type;               // Point, spot, directional
    float direction[3];          // For spot/directional
    float cone_angle;            // For spot lights
};

#pragma pack(pop)

// Parsed chunk object
struct ChunkObject {
    std::string name;
    ChunkObjectType type;
    uint32_t flags;

    // Transform
    float position[3]{0, 0, 0};
    float rotation[4]{0, 0, 0, 1}; // Quaternion
    float scale[3]{1, 1, 1};

    // References
    int32_t mesh_index = -1;
    int32_t material_index = -1;
    int32_t collision_index = -1;

    // Object-specific data
    std::vector<uint8_t> extra_data;
};

// Parsed chunk data
struct ChunkData {
    std::string name;
    uint32_t zone_id;
    uint32_t lod_level;

    // World bounds
    float bounds_min[3];
    float bounds_max[3];
    float origin[3];

    // Objects in this chunk
    std::vector<ChunkObject> objects;

    // Geometry data
    std::vector<std::shared_ptr<MeshData>> meshes;

    // Lights
    std::vector<ChunkLight> lights;

    // Material/texture references
    std::vector<std::string> textures;
};

// World chunk archive
class WorldChunk {
public:
    WorldChunk() = default;
    ~WorldChunk() = default;

    // Open a chunk file
    bool open(const std::filesystem::path& path);

    // Close the chunk
    void close();

    // Get chunk data
    const ChunkData& data() const { return m_data; }

    // Check if open
    bool isOpen() const { return m_isOpen; }

    // Get zone ID
    uint32_t zoneId() const { return m_data.zone_id; }

    // Get LOD level
    uint32_t lodLevel() const { return m_data.lod_level; }

    // Get world-space bounds
    void getBounds(float* min, float* max) const;

    // Check if point is inside chunk bounds
    bool containsPoint(float x, float y, float z) const;

    // Get all objects of a specific type
    std::vector<const ChunkObject*> getObjectsByType(ChunkObjectType type) const;

    // Export chunk geometry to OBJ
    bool exportOBJ(const std::filesystem::path& path) const;

private:
    std::filesystem::path m_path;
    ChunkData m_data;
    bool m_isOpen = false;

    bool parseHeader(std::ifstream& file);
    bool parseObjects(std::ifstream& file);
    bool parseGeometry(std::ifstream& file);

    // Extract 4x4 matrix to position/rotation/scale
    void decomposeTransform(const float* matrix, ChunkObject& obj);
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
