#include "mesh.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace opensaints {

// Helper implementations

std::filesystem::path findMeshPairedFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string pairedExt;
    if (ext == ".cmesh_pc") {
        pairedExt = ".gcmesh_pc";
    } else if (ext == ".gcmesh_pc") {
        pairedExt = ".cmesh_pc";
    } else if (ext == ".smesh_pc") {
        pairedExt = ".gsmesh_pc";
    } else if (ext == ".gsmesh_pc") {
        pairedExt = ".smesh_pc";
    } else {
        return {};
    }

    auto paired = path;
    paired.replace_extension(pairedExt);

    if (std::filesystem::exists(paired)) {
        return paired;
    }

    return {};
}

bool exportMeshToOBJ(const MeshData& mesh, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to create OBJ file: " << path << "\n";
        return false;
    }

    file << "# OpenSaints Mesh Export\n";
    file << "# Mesh: " << mesh.name << "\n";
    file << "# Submeshes: " << mesh.submeshes.size() << "\n\n";

    uint32_t vertexOffset = 1; // OBJ indices are 1-based

    for (size_t si = 0; si < mesh.submeshes.size(); ++si) {
        const auto& submesh = mesh.submeshes[si];

        file << "# Submesh " << si << ": " << submesh.name << "\n";
        file << "g " << (submesh.name.empty() ? "submesh_" + std::to_string(si) : submesh.name) << "\n";

        // Write vertices
        for (const auto& v : submesh.vertices) {
            file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
        }

        // Write texture coordinates
        for (const auto& v : submesh.vertices) {
            file << "vt " << v.texcoord0.u << " " << (1.0f - v.texcoord0.v) << "\n"; // Flip V
        }

        // Write normals
        for (const auto& v : submesh.vertices) {
            file << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
        }

        // Write faces
        for (size_t i = 0; i + 2 < submesh.indices.size(); i += 3) {
            uint32_t i0 = submesh.indices[i + 0] + vertexOffset;
            uint32_t i1 = submesh.indices[i + 1] + vertexOffset;
            uint32_t i2 = submesh.indices[i + 2] + vertexOffset;

            file << "f " << i0 << "/" << i0 << "/" << i0 << " "
                 << i1 << "/" << i1 << "/" << i1 << " "
                 << i2 << "/" << i2 << "/" << i2 << "\n";
        }

        file << "\n";
        vertexOffset += static_cast<uint32_t>(submesh.vertices.size());
    }

    std::cout << "Exported mesh to: " << path << "\n";
    return true;
}

// CharacterMesh implementation

bool CharacterMesh::open(const std::filesystem::path& path) {
    auto paired = findMeshPairedFile(path);
    if (paired.empty()) {
        std::cerr << "Could not find paired mesh file for: " << path << "\n";
        return false;
    }

    std::filesystem::path cpuPath, gpuPath;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".cmesh_pc") {
        cpuPath = path;
        gpuPath = paired;
    } else {
        cpuPath = paired;
        gpuPath = path;
    }

    return open(cpuPath, gpuPath);
}

bool CharacterMesh::open(const std::filesystem::path& cpuPath,
                         const std::filesystem::path& gpuPath) {
    close();

    // Open CPU header file
    std::ifstream cpuFile(cpuPath, std::ios::binary);
    if (!cpuFile.is_open()) {
        std::cerr << "Failed to open CPU mesh file: " << cpuPath << "\n";
        return false;
    }

    m_cpuPath = cpuPath;
    m_gpuPath = gpuPath;
    m_data.name = cpuPath.stem().string();
    m_data.is_skinned = true; // Character meshes are skinned

    // Parse header and vertex/index data
    // Note: The exact format needs to be reverse-engineered
    // This is a placeholder implementation

    if (!parseHeader(cpuFile)) {
        std::cerr << "Failed to parse mesh header\n";
        return false;
    }

    // Open GPU data file
    std::ifstream gpuFile(gpuPath, std::ios::binary);
    if (!gpuFile.is_open()) {
        std::cerr << "Failed to open GPU mesh file: " << gpuPath << "\n";
        return false;
    }

    if (!parseVertices(gpuFile)) {
        std::cerr << "Failed to parse vertices\n";
        return false;
    }

    if (!parseIndices(gpuFile)) {
        std::cerr << "Failed to parse indices\n";
        return false;
    }

    m_isOpen = true;

    std::cout << "Opened character mesh: " << cpuPath.filename();
    size_t totalVerts = 0, totalTris = 0;
    for (const auto& sm : m_data.submeshes) {
        totalVerts += sm.vertices.size();
        totalTris += sm.indices.size() / 3;
    }
    std::cout << " (" << m_data.submeshes.size() << " submeshes, "
              << totalVerts << " verts, " << totalTris << " tris)\n";

    return true;
}

void CharacterMesh::close() {
    m_data = MeshData{};
    m_cpuPath.clear();
    m_gpuPath.clear();
    m_isOpen = false;
}

bool CharacterMesh::parseHeader(std::ifstream& cpuFile) {
    // Read file size to understand structure
    cpuFile.seekg(0, std::ios::end);
    size_t fileSize = cpuFile.tellg();
    cpuFile.seekg(0);

    if (fileSize < sizeof(MeshHeader)) {
        std::cerr << "File too small for mesh header\n";
        return false;
    }

    // Read raw header bytes
    MeshHeader header;
    cpuFile.read(reinterpret_cast<char*>(&header), sizeof(MeshHeader));

    // Store bounding box info
    m_data.bounding_min = {header.bounding_min[0], header.bounding_min[1], header.bounding_min[2]};
    m_data.bounding_max = {header.bounding_max[0], header.bounding_max[1], header.bounding_max[2]};
    m_data.bounding_center = {header.bounding_center[0], header.bounding_center[1], header.bounding_center[2]};
    m_data.bounding_radius = header.bounding_radius;

    // Read submesh info if present
    // Note: Actual structure needs verification
    if (header.num_submeshes > 0 && header.num_submeshes < 1000) {
        for (uint32_t i = 0; i < header.num_submeshes; ++i) {
            SubmeshInfo info;
            cpuFile.read(reinterpret_cast<char*>(&info), sizeof(SubmeshInfo));

            Submesh sm;
            sm.material_index = info.material_index;
            // Vertices/indices will be filled in GPU parsing
            m_data.submeshes.push_back(std::move(sm));
        }
    } else {
        // Create a single default submesh
        m_data.submeshes.push_back(Submesh{});
    }

    return true;
}

bool CharacterMesh::parseVertices(std::ifstream& gpuFile) {
    // Placeholder - actual vertex parsing depends on format discovery
    // For now, we'll read raw vertex data assuming a standard format

    gpuFile.seekg(0, std::ios::end);
    size_t gpuSize = gpuFile.tellg();
    gpuFile.seekg(0);

    // Typical skinned vertex: position (12) + normal (12) + uv (8) + bones (4) + weights (4) = 40 bytes
    // Or with tangent: + 12 = 52 bytes
    const size_t vertexStride = 40;

    if (gpuSize < vertexStride) {
        return true; // Empty mesh
    }

    // Try to estimate vertex count
    size_t estimatedVerts = gpuSize / vertexStride;

    if (m_data.submeshes.empty()) {
        m_data.submeshes.push_back(Submesh{});
    }

    auto& submesh = m_data.submeshes[0];
    submesh.vertices.reserve(estimatedVerts);

    // Read vertices
    std::vector<uint8_t> buffer(vertexStride);
    for (size_t i = 0; i < estimatedVerts; ++i) {
        gpuFile.read(reinterpret_cast<char*>(buffer.data()), vertexStride);
        if (!gpuFile) break;

        Vertex v;

        // Parse position (assuming float[3] at offset 0)
        std::memcpy(&v.position.x, buffer.data() + 0, 4);
        std::memcpy(&v.position.y, buffer.data() + 4, 4);
        std::memcpy(&v.position.z, buffer.data() + 8, 4);

        // Parse normal (assuming float[3] at offset 12)
        std::memcpy(&v.normal.x, buffer.data() + 12, 4);
        std::memcpy(&v.normal.y, buffer.data() + 16, 4);
        std::memcpy(&v.normal.z, buffer.data() + 20, 4);

        // Parse UV (assuming float[2] at offset 24)
        std::memcpy(&v.texcoord0.u, buffer.data() + 24, 4);
        std::memcpy(&v.texcoord0.v, buffer.data() + 28, 4);

        // Parse bone data (at offset 32)
        std::memcpy(v.bone_indices, buffer.data() + 32, 4);

        // Parse bone weights (at offset 36)
        for (int j = 0; j < 4; ++j) {
            v.bone_weights[j] = buffer[36 + j] / 255.0f;
        }

        submesh.vertices.push_back(v);
    }

    return true;
}

bool CharacterMesh::parseIndices(std::ifstream& gpuFile) {
    // Indices are typically stored after vertices
    // For now, generate a simple triangle list
    if (m_data.submeshes.empty() || m_data.submeshes[0].vertices.empty()) {
        return true;
    }

    auto& submesh = m_data.submeshes[0];
    size_t vertCount = submesh.vertices.size();

    // Generate simple triangle fan/strip as placeholder
    // Real implementation needs to read index buffer from GPU file
    submesh.indices.reserve(vertCount);
    for (size_t i = 0; i < vertCount; ++i) {
        submesh.indices.push_back(static_cast<uint32_t>(i));
    }

    return true;
}

bool CharacterMesh::exportOBJ(const std::filesystem::path& path) const {
    return exportMeshToOBJ(m_data, path);
}

// StaticMesh implementation

bool StaticMesh::open(const std::filesystem::path& path) {
    auto paired = findMeshPairedFile(path);
    if (paired.empty()) {
        std::cerr << "Could not find paired mesh file for: " << path << "\n";
        return false;
    }

    std::filesystem::path cpuPath, gpuPath;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".smesh_pc") {
        cpuPath = path;
        gpuPath = paired;
    } else {
        cpuPath = paired;
        gpuPath = path;
    }

    return open(cpuPath, gpuPath);
}

bool StaticMesh::open(const std::filesystem::path& cpuPath,
                      const std::filesystem::path& gpuPath) {
    close();

    // Open CPU header file
    std::ifstream cpuFile(cpuPath, std::ios::binary);
    if (!cpuFile.is_open()) {
        std::cerr << "Failed to open CPU mesh file: " << cpuPath << "\n";
        return false;
    }

    m_cpuPath = cpuPath;
    m_gpuPath = gpuPath;
    m_data.name = cpuPath.stem().string();
    m_data.is_skinned = false; // Static meshes are not skinned

    // Read file size
    cpuFile.seekg(0, std::ios::end);
    size_t cpuSize = cpuFile.tellg();
    cpuFile.seekg(0);

    if (cpuSize < sizeof(MeshHeader)) {
        std::cerr << "File too small for mesh header\n";
        return false;
    }

    // Read header
    MeshHeader header;
    cpuFile.read(reinterpret_cast<char*>(&header), sizeof(MeshHeader));

    m_data.bounding_min = {header.bounding_min[0], header.bounding_min[1], header.bounding_min[2]};
    m_data.bounding_max = {header.bounding_max[0], header.bounding_max[1], header.bounding_max[2]};
    m_data.bounding_center = {header.bounding_center[0], header.bounding_center[1], header.bounding_center[2]};
    m_data.bounding_radius = header.bounding_radius;

    // Open GPU file
    std::ifstream gpuFile(gpuPath, std::ios::binary);
    if (!gpuFile.is_open()) {
        std::cerr << "Failed to open GPU mesh file: " << gpuPath << "\n";
        return false;
    }

    gpuFile.seekg(0, std::ios::end);
    size_t gpuSize = gpuFile.tellg();
    gpuFile.seekg(0);

    // Static mesh vertex: position (12) + normal (12) + uv (8) = 32 bytes typical
    const size_t vertexStride = 32;

    if (gpuSize >= vertexStride) {
        size_t estimatedVerts = gpuSize / vertexStride;

        Submesh submesh;
        submesh.vertices.reserve(estimatedVerts);

        std::vector<uint8_t> buffer(vertexStride);
        for (size_t i = 0; i < estimatedVerts; ++i) {
            gpuFile.read(reinterpret_cast<char*>(buffer.data()), vertexStride);
            if (!gpuFile) break;

            Vertex v;

            std::memcpy(&v.position.x, buffer.data() + 0, 4);
            std::memcpy(&v.position.y, buffer.data() + 4, 4);
            std::memcpy(&v.position.z, buffer.data() + 8, 4);

            std::memcpy(&v.normal.x, buffer.data() + 12, 4);
            std::memcpy(&v.normal.y, buffer.data() + 16, 4);
            std::memcpy(&v.normal.z, buffer.data() + 20, 4);

            std::memcpy(&v.texcoord0.u, buffer.data() + 24, 4);
            std::memcpy(&v.texcoord0.v, buffer.data() + 28, 4);

            submesh.vertices.push_back(v);
        }

        // Generate indices
        for (size_t i = 0; i < submesh.vertices.size(); ++i) {
            submesh.indices.push_back(static_cast<uint32_t>(i));
        }

        m_data.submeshes.push_back(std::move(submesh));
    }

    m_isOpen = true;

    std::cout << "Opened static mesh: " << cpuPath.filename();
    size_t totalVerts = 0, totalTris = 0;
    for (const auto& sm : m_data.submeshes) {
        totalVerts += sm.vertices.size();
        totalTris += sm.indices.size() / 3;
    }
    std::cout << " (" << m_data.submeshes.size() << " submeshes, "
              << totalVerts << " verts, " << totalTris << " tris)\n";

    return true;
}

void StaticMesh::close() {
    m_data = MeshData{};
    m_cpuPath.clear();
    m_gpuPath.clear();
    m_isOpen = false;
}

bool StaticMesh::exportOBJ(const std::filesystem::path& path) const {
    return exportMeshToOBJ(m_data, path);
}

} // namespace opensaints
