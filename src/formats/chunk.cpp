#include "chunk.h"
#include "mesh.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace opensaints {

// WorldChunk implementation

bool WorldChunk::open(const std::filesystem::path& path) {
    close();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open chunk file: " << path << "\n";
        return false;
    }

    m_path = path;
    m_data.name = path.stem().string();

    if (!parseHeader(file)) {
        std::cerr << "Failed to parse chunk header\n";
        return false;
    }

    if (!parseObjects(file)) {
        std::cerr << "Failed to parse chunk objects\n";
        return false;
    }

    if (!parseGeometry(file)) {
        std::cerr << "Failed to parse chunk geometry\n";
        return false;
    }

    m_isOpen = true;

    std::cout << "Opened chunk: " << path.filename()
              << " (zone " << m_data.zone_id
              << ", LOD " << m_data.lod_level
              << ", " << m_data.objects.size() << " objects)\n";

    return true;
}

void WorldChunk::close() {
    m_data = ChunkData{};
    m_path.clear();
    m_isOpen = false;
}

bool WorldChunk::parseHeader(std::ifstream& file) {
    // Read file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0);

    if (fileSize < sizeof(ChunkHeader)) {
        std::cerr << "File too small for chunk header\n";
        return false;
    }

    ChunkHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(ChunkHeader));

    // Store chunk info
    m_data.zone_id = header.zone_id;
    m_data.lod_level = header.lod_level;

    std::memcpy(m_data.bounds_min, header.bounds_min, sizeof(float) * 3);
    std::memcpy(m_data.bounds_max, header.bounds_max, sizeof(float) * 3);
    std::memcpy(m_data.origin, header.origin, sizeof(float) * 3);

    return true;
}

bool WorldChunk::parseObjects(std::ifstream& file) {
    // Placeholder implementation
    // Real implementation needs format reverse-engineering

    // For now, create some dummy objects to demonstrate the structure
    ChunkObject obj;
    obj.name = "terrain_" + m_data.name;
    obj.type = ChunkObjectType::Terrain;
    obj.flags = 0;
    obj.position[0] = m_data.origin[0];
    obj.position[1] = m_data.origin[1];
    obj.position[2] = m_data.origin[2];
    m_data.objects.push_back(std::move(obj));

    return true;
}

bool WorldChunk::parseGeometry(std::ifstream& file) {
    // Placeholder implementation
    // Real geometry parsing depends on discovered format

    return true;
}

void WorldChunk::decomposeTransform(const float* matrix, ChunkObject& obj) {
    // Extract position from last column
    obj.position[0] = matrix[12];
    obj.position[1] = matrix[13];
    obj.position[2] = matrix[14];

    // Extract scale from column lengths
    obj.scale[0] = std::sqrt(matrix[0] * matrix[0] + matrix[1] * matrix[1] + matrix[2] * matrix[2]);
    obj.scale[1] = std::sqrt(matrix[4] * matrix[4] + matrix[5] * matrix[5] + matrix[6] * matrix[6]);
    obj.scale[2] = std::sqrt(matrix[8] * matrix[8] + matrix[9] * matrix[9] + matrix[10] * matrix[10]);

    // Extract rotation as quaternion (simplified - assumes orthogonal matrix)
    // Using a simplified rotation matrix to quaternion conversion
    float trace = matrix[0] / obj.scale[0] + matrix[5] / obj.scale[1] + matrix[10] / obj.scale[2];

    if (trace > 0) {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        obj.rotation[3] = 0.25f / s;
        obj.rotation[0] = (matrix[6] / obj.scale[1] - matrix[9] / obj.scale[2]) * s;
        obj.rotation[1] = (matrix[8] / obj.scale[2] - matrix[2] / obj.scale[0]) * s;
        obj.rotation[2] = (matrix[1] / obj.scale[0] - matrix[4] / obj.scale[1]) * s;
    } else {
        // Default to identity
        obj.rotation[0] = 0;
        obj.rotation[1] = 0;
        obj.rotation[2] = 0;
        obj.rotation[3] = 1;
    }
}

void WorldChunk::getBounds(float* min, float* max) const {
    std::memcpy(min, m_data.bounds_min, sizeof(float) * 3);
    std::memcpy(max, m_data.bounds_max, sizeof(float) * 3);
}

bool WorldChunk::containsPoint(float x, float y, float z) const {
    return x >= m_data.bounds_min[0] && x <= m_data.bounds_max[0] &&
           y >= m_data.bounds_min[1] && y <= m_data.bounds_max[1] &&
           z >= m_data.bounds_min[2] && z <= m_data.bounds_max[2];
}

std::vector<const ChunkObject*> WorldChunk::getObjectsByType(ChunkObjectType type) const {
    std::vector<const ChunkObject*> result;
    for (const auto& obj : m_data.objects) {
        if (obj.type == type) {
            result.push_back(&obj);
        }
    }
    return result;
}

bool WorldChunk::exportOBJ(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to create OBJ file: " << path << "\n";
        return false;
    }

    file << "# OpenSaints Chunk Export\n";
    file << "# Chunk: " << m_data.name << "\n";
    file << "# Zone: " << m_data.zone_id << "\n";
    file << "# LOD: " << m_data.lod_level << "\n";
    file << "# Objects: " << m_data.objects.size() << "\n\n";

    uint32_t vertexOffset = 1;

    // Export each mesh in the chunk
    for (const auto& meshPtr : m_data.meshes) {
        if (!meshPtr) continue;

        const auto& mesh = *meshPtr;
        for (size_t si = 0; si < mesh.submeshes.size(); ++si) {
            const auto& submesh = mesh.submeshes[si];

            file << "g " << mesh.name << "_submesh" << si << "\n";

            for (const auto& v : submesh.vertices) {
                file << "v " << v.position.x << " " << v.position.y << " " << v.position.z << "\n";
            }

            for (const auto& v : submesh.vertices) {
                file << "vt " << v.texcoord0.u << " " << (1.0f - v.texcoord0.v) << "\n";
            }

            for (const auto& v : submesh.vertices) {
                file << "vn " << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
            }

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
    }

    std::cout << "Exported chunk to: " << path << "\n";
    return true;
}

// ZoneManager implementation

bool ZoneManager::loadZoneDefinitions(const std::filesystem::path& path) {
    // Placeholder - zone definitions would come from XTBL files
    std::cout << "Zone definitions would be loaded from: " << path << "\n";

    // Create some default zones for Stilwater
    ZoneInfo downtown;
    downtown.id = 1;
    downtown.name = "Downtown";
    downtown.bounds_min[0] = -1000; downtown.bounds_min[1] = -1000; downtown.bounds_min[2] = 0;
    downtown.bounds_max[0] = 1000; downtown.bounds_max[1] = 1000; downtown.bounds_max[2] = 500;
    m_zones.push_back(downtown);

    ZoneInfo suburbs;
    suburbs.id = 2;
    suburbs.name = "Suburbs";
    suburbs.bounds_min[0] = 1000; suburbs.bounds_min[1] = -1000; suburbs.bounds_min[2] = 0;
    suburbs.bounds_max[0] = 3000; suburbs.bounds_max[1] = 1000; suburbs.bounds_max[2] = 200;
    m_zones.push_back(suburbs);

    return true;
}

std::vector<std::string> ZoneManager::getChunksForPosition(float x, float y, float z) const {
    std::vector<std::string> chunks;

    for (const auto& zone : m_zones) {
        if (x >= zone.bounds_min[0] && x <= zone.bounds_max[0] &&
            y >= zone.bounds_min[1] && y <= zone.bounds_max[1] &&
            z >= zone.bounds_min[2] && z <= zone.bounds_max[2]) {
            for (const auto& chunk : zone.chunk_files) {
                chunks.push_back(chunk);
            }
        }
    }

    return chunks;
}

uint32_t ZoneManager::getZoneIdForPosition(float x, float y, float z) const {
    for (const auto& zone : m_zones) {
        if (x >= zone.bounds_min[0] && x <= zone.bounds_max[0] &&
            y >= zone.bounds_min[1] && y <= zone.bounds_max[1] &&
            z >= zone.bounds_min[2] && z <= zone.bounds_max[2]) {
            return zone.id;
        }
    }
    return 0; // Unknown zone
}

std::string ZoneManager::getZoneName(uint32_t zoneId) const {
    for (const auto& zone : m_zones) {
        if (zone.id == zoneId) {
            return zone.name;
        }
    }
    return "Unknown";
}

} // namespace opensaints
