#pragma once
// Mesh parser for Saints Row 2
// Handles .cmesh_pc, .gcmesh_pc (character meshes) and .smesh_pc, .gsmesh_pc (static meshes)
// Similar to PEG, meshes are split into CPU header and GPU data files

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <fstream>

namespace opensaints {

// Mesh file signatures
constexpr uint32_t CMESH_SIGNATURE = 0x424BD00D; // Character mesh "..KB"
constexpr uint32_t SMESH_SIGNATURE = 0x424BD00D; // Static mesh (same signature)

// Vertex component types
enum class VertexComponentType : uint8_t {
    Position = 0,
    Normal = 1,
    Tangent = 2,
    Binormal = 3,
    TexCoord0 = 4,
    TexCoord1 = 5,
    Color = 6,
    BoneIndices = 7,
    BoneWeights = 8
};

// Index format
enum class IndexFormat : uint8_t {
    UInt16 = 0,
    UInt32 = 1
};

#pragma pack(push, 1)

// Mesh header structure (preliminary - needs verification)
struct MeshHeader {
    uint32_t signature;          // File signature
    uint32_t version;            // Format version
    uint32_t flags;              // Mesh flags
    uint32_t num_submeshes;      // Number of submeshes/LODs
    uint32_t num_vertices;       // Total vertex count
    uint32_t num_indices;        // Total index count
    uint32_t vertex_stride;      // Bytes per vertex
    uint32_t vertex_format;      // Vertex format flags
    float bounding_min[3];       // Bounding box minimum
    float bounding_max[3];       // Bounding box maximum
    float bounding_center[3];    // Bounding sphere center
    float bounding_radius;       // Bounding sphere radius
};

// Submesh/material group
struct SubmeshInfo {
    uint32_t start_index;        // Start index in index buffer
    uint32_t num_indices;        // Number of indices
    uint32_t start_vertex;       // Start vertex in vertex buffer
    uint32_t num_vertices;       // Number of vertices
    uint32_t material_index;     // Material/texture reference
    uint32_t flags;              // Submesh flags
};

// Bone weight data for skinned meshes
struct BoneWeight {
    uint8_t bone_indices[4];     // Bone indices (0-255)
    uint8_t weights[4];          // Bone weights (0-255, normalized)
};

#pragma pack(pop)

// 3D Vector
struct Vec3 {
    float x, y, z;
};

// 2D Vector (UV)
struct Vec2 {
    float u, v;
};

// Vertex with all possible components
struct Vertex {
    Vec3 position{0, 0, 0};
    Vec3 normal{0, 1, 0};
    Vec3 tangent{1, 0, 0};
    Vec3 binormal{0, 0, 1};
    Vec2 texcoord0{0, 0};
    Vec2 texcoord1{0, 0};
    uint32_t color{0xFFFFFFFF};
    uint8_t bone_indices[4]{0, 0, 0, 0};
    float bone_weights[4]{0, 0, 0, 0};
};

// A single submesh
struct Submesh {
    std::string name;
    uint32_t material_index;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Vec3 bounding_min;
    Vec3 bounding_max;
};

// Material reference
struct MeshMaterial {
    std::string name;
    std::string diffuse_texture;
    std::string normal_texture;
    std::string specular_texture;
    uint32_t flags;
};

// Complete mesh data
struct MeshData {
    std::string name;
    std::vector<Submesh> submeshes;
    std::vector<MeshMaterial> materials;
    Vec3 bounding_min;
    Vec3 bounding_max;
    Vec3 bounding_center;
    float bounding_radius;

    // Is this a skinned mesh?
    bool is_skinned;

    // Bone names for skinned meshes
    std::vector<std::string> bone_names;
};

// Character mesh archive (cmesh)
class CharacterMesh {
public:
    CharacterMesh() = default;
    ~CharacterMesh() = default;

    // Open a character mesh (pass .cmesh_pc or .gcmesh_pc)
    bool open(const std::filesystem::path& path);

    // Open with explicit paths
    bool open(const std::filesystem::path& cpuPath,
              const std::filesystem::path& gpuPath);

    // Close the mesh
    void close();

    // Get mesh data
    const MeshData& data() const { return m_data; }

    // Check if open
    bool isOpen() const { return m_isOpen; }

    // Export to OBJ format
    bool exportOBJ(const std::filesystem::path& path) const;

private:
    std::filesystem::path m_cpuPath;
    std::filesystem::path m_gpuPath;
    MeshData m_data;
    bool m_isOpen = false;

    bool parseHeader(std::ifstream& cpuFile);
    bool parseVertices(std::ifstream& gpuFile);
    bool parseIndices(std::ifstream& gpuFile);
};

// Static mesh archive (smesh)
class StaticMesh {
public:
    StaticMesh() = default;
    ~StaticMesh() = default;

    // Open a static mesh
    bool open(const std::filesystem::path& path);
    bool open(const std::filesystem::path& cpuPath,
              const std::filesystem::path& gpuPath);

    void close();

    const MeshData& data() const { return m_data; }
    bool isOpen() const { return m_isOpen; }

    bool exportOBJ(const std::filesystem::path& path) const;

private:
    std::filesystem::path m_cpuPath;
    std::filesystem::path m_gpuPath;
    MeshData m_data;
    bool m_isOpen = false;
};

// Helper to find paired mesh file
std::filesystem::path findMeshPairedFile(const std::filesystem::path& path);

// Export mesh to OBJ format
bool exportMeshToOBJ(const MeshData& mesh, const std::filesystem::path& path);

} // namespace opensaints
