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
        pairedExt = ".g_cmesh_pc";
    } else if (ext == ".g_cmesh_pc") {
        pairedExt = ".cmesh_pc";
    } else if (ext == ".smesh_pc") {
        pairedExt = ".g_smesh_pc";
    } else if (ext == ".g_smesh_pc") {
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
    } else if (ext == ".g_cmesh_pc") {
        cpuPath = paired;
        gpuPath = path;
    } else {
        std::cerr << "Unknown mesh extension: " << ext << "\n";
        return false;
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

    if (fileSize < 16) {
        std::cerr << "File too small for mesh header\n";
        return false;
    }

    // Read signature
    uint32_t signature;
    cpuFile.read(reinterpret_cast<char*>(&signature), 4);

    if (signature != CMESH_SIGNATURE) {
        std::cerr << "Invalid mesh signature: 0x" << std::hex << signature
                  << " (expected 0x" << CMESH_SIGNATURE << ")\n" << std::dec;
        return false;
    }

    // Read version and flags
    uint32_t version;
    cpuFile.read(reinterpret_cast<char*>(&version), 4);
    std::cout << "Mesh version: 0x" << std::hex << version << std::dec << "\n";

    // Skip to material names section (typically after header)
    // Look for texture names in the file
    cpuFile.seekg(0x60); // Texture names often start around here
    std::vector<char> textureBlock(256);
    cpuFile.read(textureBlock.data(), 256);

    // Find texture names (null-terminated strings)
    std::vector<std::string> texNames;
    for (size_t i = 0; i < textureBlock.size();) {
        if (textureBlock[i] != '\0') {
            std::string name(&textureBlock[i]);
            if (name.length() > 4 && name.length() < 64) {
                // Check for .tga or .tgn extension
                if (name.find(".tga") != std::string::npos ||
                    name.find(".tgn") != std::string::npos) {
                    texNames.push_back(name);
                }
            }
            i += name.length() + 1;
        } else {
            i++;
        }
    }

    if (!texNames.empty()) {
        std::cout << "Found " << texNames.size() << " texture references:\n";
        for (const auto& t : texNames) {
            std::cout << "  - " << t << "\n";
            MeshMaterial mat;
            mat.diffuse_texture = t;
            m_data.materials.push_back(mat);
        }
    }

    // Create a single default submesh
    m_data.submeshes.push_back(Submesh{});

    return true;
}

bool CharacterMesh::parseVertices(std::ifstream& gpuFile) {
    gpuFile.seekg(0, std::ios::end);
    size_t gpuSize = gpuFile.tellg();
    gpuFile.seekg(0);

    if (gpuSize < 16) {
        return true; // Empty mesh
    }

    // SR2 character mesh vertex format (40 bytes):
    // - Position: 3 floats (12 bytes) at offset 0
    // - Color/flags: 4 bytes at offset 12
    // - Unknown: 4 bytes at offset 16
    // - Packed normal: 4 bytes at offset 20
    // - Packed tangent: 4 bytes at offset 24
    // - UV (16-bit fixed): 4 bytes at offset 28
    // - UV2 (16-bit fixed): 4 bytes at offset 32
    // - Bone data: 4 bytes at offset 36
    const size_t vertexStride = 40;

    size_t numVerts = gpuSize / vertexStride;
    std::cout << "GPU file: " << gpuSize << " bytes, estimated " << numVerts << " vertices (stride " << vertexStride << ")\n";

    if (m_data.submeshes.empty()) {
        m_data.submeshes.push_back(Submesh{});
    }

    auto& submesh = m_data.submeshes[0];
    submesh.vertices.reserve(numVerts);

    std::vector<uint8_t> buffer(vertexStride);
    for (size_t i = 0; i < numVerts; ++i) {
        gpuFile.read(reinterpret_cast<char*>(buffer.data()), vertexStride);
        if (!gpuFile) break;

        Vertex v;

        // Position (float[3] at offset 0)
        std::memcpy(&v.position.x, buffer.data() + 0, 4);
        std::memcpy(&v.position.y, buffer.data() + 4, 4);
        std::memcpy(&v.position.z, buffer.data() + 8, 4);

        // Packed normal at offset 20 (4 bytes, each component 0-255 maps to -1 to 1)
        v.normal.x = (buffer[20] / 127.5f) - 1.0f;
        v.normal.y = (buffer[21] / 127.5f) - 1.0f;
        v.normal.z = (buffer[22] / 127.5f) - 1.0f;

        // UV at offset 28 (16-bit fixed point, need to decode)
        // Format appears to be 16-bit signed fixed point
        int16_t u16, v16;
        std::memcpy(&u16, buffer.data() + 28, 2);
        std::memcpy(&v16, buffer.data() + 30, 2);
        v.texcoord0.u = u16 / 1024.0f;  // Rough scale factor
        v.texcoord0.v = v16 / 1024.0f;

        // Bone indices at offset 36
        std::memcpy(v.bone_indices, buffer.data() + 36, 4);

        // Color at offset 12 (RGBA)
        v.color = buffer[12] | (buffer[13] << 8) | (buffer[14] << 16) | (buffer[15] << 24);

        submesh.vertices.push_back(v);
    }

    return true;
}

bool CharacterMesh::parseIndices(std::ifstream& gpuFile) {
    // Indices are stored in the CPU file, not GPU file
    // We need to read them from m_cpuPath
    if (m_data.submeshes.empty() || m_data.submeshes[0].vertices.empty()) {
        return true;
    }

    std::ifstream cpuFile(m_cpuPath, std::ios::binary);
    if (!cpuFile.is_open()) {
        // Fall back to sequential indices
        auto& submesh = m_data.submeshes[0];
        for (size_t i = 0; i < submesh.vertices.size(); ++i) {
            submesh.indices.push_back(static_cast<uint32_t>(i));
        }
        return true;
    }

    // Scan CPU file for index data (look for sequential 32-bit integers)
    cpuFile.seekg(0, std::ios::end);
    size_t cpuSize = cpuFile.tellg();
    cpuFile.seekg(0);

    // Read entire CPU file
    std::vector<uint8_t> cpuData(cpuSize);
    cpuFile.read(reinterpret_cast<char*>(cpuData.data()), cpuSize);

    // Look for index buffer (typically after header, contains sequential or near-sequential values)
    // In the privacy mesh, indices started at offset 0x50
    size_t indexOffset = 0x50;
    auto& submesh = m_data.submeshes[0];

    // Try to find valid index data
    if (indexOffset + 12 <= cpuSize) {
        // Check if this looks like 32-bit indices (0, 1, 2, ...)
        uint32_t first3[3];
        std::memcpy(first3, cpuData.data() + indexOffset, 12);

        // If first 3 values are 0, 1, 2 or small sequential values, it's likely indices
        if (first3[0] < submesh.vertices.size() &&
            first3[1] < submesh.vertices.size() &&
            first3[2] < submesh.vertices.size()) {

            // Read indices until we hit invalid data or end of reasonable range
            size_t maxIndices = (cpuSize - indexOffset) / 4;
            for (size_t i = 0; i < maxIndices; ++i) {
                uint32_t idx;
                std::memcpy(&idx, cpuData.data() + indexOffset + i * 4, 4);

                // Stop if index is out of range or looks like other data
                if (idx >= submesh.vertices.size() || idx == 0xFFFFFFFF) {
                    break;
                }

                submesh.indices.push_back(idx);
            }

            std::cout << "Read " << submesh.indices.size() << " indices from CPU file\n";
        }
    }

    // Fall back if no indices found
    if (submesh.indices.empty()) {
        for (size_t i = 0; i < submesh.vertices.size(); ++i) {
            submesh.indices.push_back(static_cast<uint32_t>(i));
        }
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
