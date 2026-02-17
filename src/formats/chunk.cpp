#include "chunk.h"
#include "mesh.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace opensaints {

// Helper to read a little-endian value from a byte buffer
template<typename T>
static T readLE(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + sizeof(T) > data.size()) return T{};
    T val;
    std::memcpy(&val, data.data() + offset, sizeof(T));
    return val;
}

// WorldChunk implementation

bool WorldChunk::open(const std::filesystem::path& path) {
    close();

    // Read CPU file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open chunk file: " << path << "\n";
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> cpuData(fileSize);
    file.read(reinterpret_cast<char*>(cpuData.data()), fileSize);
    file.close();

    // Try to find paired GPU file
    std::vector<uint8_t> gpuData;
    std::filesystem::path gpuPath = path.parent_path() / (path.stem().string() + ".g_chunk_pc");
    if (std::filesystem::exists(gpuPath)) {
        std::ifstream gpuFile(gpuPath, std::ios::binary | std::ios::ate);
        if (gpuFile.is_open()) {
            size_t gpuSize = gpuFile.tellg();
            gpuFile.seekg(0);
            gpuData.resize(gpuSize);
            gpuFile.read(reinterpret_cast<char*>(gpuData.data()), gpuSize);
        }
    }

    m_path = path;
    m_data.name = path.stem().string();

    if (!parseFromBuffer(cpuData, gpuData)) {
        return false;
    }

    m_isOpen = true;

    std::cout << "Opened chunk: " << path.filename()
              << " (" << m_data.textures.size() << " textures"
              << ", " << m_data.render_item_count << " render items"
              << ", VB=" << m_data.gpu_vertex_size
              << ", IB=" << m_data.gpu_index_size << ")\n";

    return true;
}

bool WorldChunk::openFromMemory(const std::string& name,
                                 const std::vector<uint8_t>& cpuData,
                                 const std::vector<uint8_t>& gpuData) {
    close();
    m_data.name = name;

    if (!parseFromBuffer(cpuData, gpuData)) {
        return false;
    }

    m_isOpen = true;
    return true;
}

void WorldChunk::close() {
    m_data = ChunkData{};
    m_path.clear();
    m_isOpen = false;
}

bool WorldChunk::parseFromBuffer(const std::vector<uint8_t>& cpuData,
                                  const std::vector<uint8_t>& gpuData) {
    if (!parseHeader(cpuData)) {
        std::cerr << "Failed to parse chunk header: " << m_data.name << "\n";
        return false;
    }

    if (!parseTextures(cpuData)) {
        // Non-fatal: some chunks have no textures
    }

    if (!gpuData.empty()) {
        if (!parseGeometry(gpuData)) {
            std::cerr << "Failed to parse chunk geometry: " << m_data.name << "\n";
            // Non-fatal: chunk is still usable for bounds/textures
        }
    }

    return true;
}

bool WorldChunk::parseHeader(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(ChunkFileHeader) + sizeof(ChunkGeometryInfo)) {
        std::cerr << "File too small for chunk header (" << data.size() << " bytes)\n";
        return false;
    }

    // Read file header
    ChunkFileHeader header;
    std::memcpy(&header, data.data(), sizeof(header));

    if (header.signature != CHUNK_SIGNATURE) {
        std::cerr << "Bad chunk signature: 0x" << std::hex << header.signature
                  << " (expected 0x" << CHUNK_SIGNATURE << ")\n" << std::dec;
        return false;
    }

    m_data.version = header.version;
    m_data.flags = header.flags;
    m_data.section_count = header.section_count;

    // Read geometry info (immediately follows file header)
    ChunkGeometryInfo geoInfo;
    std::memcpy(&geoInfo, data.data() + sizeof(ChunkFileHeader), sizeof(geoInfo));

    m_data.gpu_vertex_size = geoInfo.gpu_vertex_size;
    m_data.gpu_index_size = geoInfo.gpu_index_size;
    m_data.render_item_count = geoInfo.render_item_count;

    std::memcpy(m_data.bounds_min, geoInfo.bounds_min, sizeof(float) * 3);
    std::memcpy(m_data.bounds_max, geoInfo.bounds_max, sizeof(float) * 3);

    return true;
}

bool WorldChunk::parseTextures(const std::vector<uint8_t>& data) {
    // Texture count at offset 0x100
    if (data.size() < 0x108) return false;

    uint32_t texCount = readLE<uint32_t>(data, 0x100);
    if (texCount == 0) return true;
    if (texCount > 10000) {
        std::cerr << "Implausible texture count: " << texCount << "\n";
        return false;
    }

    // Texture names are null-terminated strings somewhere after 0x108.
    // There may be a gap of zero bytes (padding/offset table). Scan for
    // the first printable ASCII character.
    size_t nameStart = 0x108;
    while (nameStart < data.size() && data[nameStart] == 0) {
        nameStart++;
    }

    size_t offset = nameStart;
    for (uint32_t i = 0; i < texCount && offset < data.size(); i++) {
        // Find null terminator
        size_t end = offset;
        while (end < data.size() && data[end] != 0) {
            end++;
        }
        if (end > offset) {
            m_data.textures.emplace_back(
                reinterpret_cast<const char*>(data.data() + offset),
                end - offset);
        }
        offset = end + 1;
    }

    return true;
}

// Phase 3: GPU geometry parsing

void WorldChunk::decodePackedNormal(uint32_t packed, float& nx, float& ny, float& nz) {
    // Packed as 4 unsigned bytes: (x, y, z, w) each 0-255
    // Map to [-1, 1]: value/127.5 - 1.0
    uint8_t bx = (packed >> 0) & 0xFF;
    uint8_t by = (packed >> 8) & 0xFF;
    uint8_t bz = (packed >> 16) & 0xFF;

    nx = (bx / 127.5f) - 1.0f;
    ny = (by / 127.5f) - 1.0f;
    nz = (bz / 127.5f) - 1.0f;

    // Normalize
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.001f) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = 0; ny = 1; nz = 0;
    }
}

bool WorldChunk::parseGeometry(const std::vector<uint8_t>& gpuData) {
    if (gpuData.empty()) return false;

    uint32_t vbSize = m_data.gpu_vertex_size;
    uint32_t ibSize = m_data.gpu_index_size;

    // Validate sizes against actual GPU file
    uint32_t totalExpected = vbSize + ibSize;
    if (totalExpected == 0) {
        // No geometry in this chunk
        return true;
    }

    // Allow slight mismatch (some chunks may have padding)
    if (gpuData.size() < totalExpected) {
        // Fall back: treat entire GPU data as vertex buffer
        vbSize = static_cast<uint32_t>(gpuData.size());
        ibSize = 0;
    }

    // Split GPU data into vertex and index buffers
    if (vbSize > 0) {
        m_data.gpu_vertex_data.assign(gpuData.begin(),
                                       gpuData.begin() + std::min<size_t>(vbSize, gpuData.size()));
    }
    if (ibSize > 0 && vbSize + ibSize <= gpuData.size()) {
        m_data.gpu_index_data.assign(gpuData.begin() + vbSize,
                                      gpuData.begin() + vbSize + ibSize);
    }

    // Vertex positions are in LOCAL SPACE relative to bounds center.
    // For stride detection, check if position magnitudes are plausible
    // (within the half-extents of the bounding box + generous padding).
    float halfExtX = (m_data.bounds_max[0] - m_data.bounds_min[0]) * 0.5f + 500.0f;
    float halfExtY = (m_data.bounds_max[1] - m_data.bounds_min[1]) * 0.5f + 500.0f;
    float halfExtZ = (m_data.bounds_max[2] - m_data.bounds_min[2]) * 0.5f + 500.0f;

    // Determine best vertex stride by testing which produces valid float3 positions
    int bestStride = 20; // Default: float3 pos + uint32 normal + uint16x2 UV
    int strideCandidates[] = {20, 24, 28, 32, 36, 40};

    int bestScore = -1;
    for (int stride : strideCandidates) {
        uint32_t vertCount = vbSize / stride;
        if (vertCount == 0 || vertCount > 10000000) continue;

        int inBounds = 0;
        int checkCount = std::min(vertCount, 200u);
        for (int i = 0; i < checkCount; i++) {
            size_t off = static_cast<size_t>(i) * stride;
            if (off + 12 > m_data.gpu_vertex_data.size()) break;

            float x, y, z;
            std::memcpy(&x, m_data.gpu_vertex_data.data() + off, 4);
            std::memcpy(&y, m_data.gpu_vertex_data.data() + off + 4, 4);
            std::memcpy(&z, m_data.gpu_vertex_data.data() + off + 8, 4);

            // Check if position is finite and within local-space half-extents
            if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
                std::abs(x) < halfExtX && std::abs(y) < halfExtY && std::abs(z) < halfExtZ) {
                inBounds++;
            }
        }

        if (inBounds > bestScore) {
            bestScore = inBounds;
            bestStride = stride;
        }
    }

    // Build mesh data - truncate to whole vertex count
    uint32_t vertexCount = vbSize / bestStride;
    if (vertexCount == 0) {
        return true; // No geometry to parse
    }

    // Bounds center used to offset local-space vertices to world-space
    float centerX = (m_data.bounds_min[0] + m_data.bounds_max[0]) * 0.5f;
    float centerY = (m_data.bounds_min[1] + m_data.bounds_max[1]) * 0.5f;
    float centerZ = (m_data.bounds_min[2] + m_data.bounds_max[2]) * 0.5f;

    auto mesh = std::make_shared<MeshData>();
    mesh->name = m_data.name;
    mesh->is_skinned = false;

    std::memcpy(&mesh->bounding_min, m_data.bounds_min, sizeof(float) * 3);
    std::memcpy(&mesh->bounding_max, m_data.bounds_max, sizeof(float) * 3);
    mesh->bounding_center = Vec3{centerX, centerY, centerZ};
    float dx = m_data.bounds_max[0] - m_data.bounds_min[0];
    float dy = m_data.bounds_max[1] - m_data.bounds_min[1];
    float dz = m_data.bounds_max[2] - m_data.bounds_min[2];
    mesh->bounding_radius = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.5f;

    // Decode vertices
    Submesh submesh;
    submesh.name = m_data.name + "_geo";
    submesh.material_index = 0;
    submesh.vertices.reserve(vertexCount);

    for (uint32_t i = 0; i < vertexCount; i++) {
        size_t off = static_cast<size_t>(i) * bestStride;
        if (off + 12 > m_data.gpu_vertex_data.size()) break;

        Vertex v;

        // Position: float3 at offset 0 (LOCAL SPACE)
        float lx, ly, lz;
        std::memcpy(&lx, m_data.gpu_vertex_data.data() + off, 4);
        std::memcpy(&ly, m_data.gpu_vertex_data.data() + off + 4, 4);
        std::memcpy(&lz, m_data.gpu_vertex_data.data() + off + 8, 4);

        // Skip vertices with non-finite positions (garbage from misaligned submeshes)
        if (!std::isfinite(lx) || !std::isfinite(ly) || !std::isfinite(lz)) {
            continue;
        }

        // Transform to world space by adding bounds center
        v.position.x = lx + centerX;
        v.position.y = ly + centerY;
        v.position.z = lz + centerZ;

        // Packed normal at offset 12 (if stride >= 16)
        if (bestStride >= 16 && off + 16 <= m_data.gpu_vertex_data.size()) {
            uint32_t packedNormal;
            std::memcpy(&packedNormal, m_data.gpu_vertex_data.data() + off + 12, 4);
            decodePackedNormal(packedNormal, v.normal.x, v.normal.y, v.normal.z);
        }

        // UV at offset 16 as two uint16 (if stride >= 20)
        if (bestStride >= 20 && off + 20 <= m_data.gpu_vertex_data.size()) {
            uint16_t u16, v16;
            std::memcpy(&u16, m_data.gpu_vertex_data.data() + off + 16, 2);
            std::memcpy(&v16, m_data.gpu_vertex_data.data() + off + 18, 2);
            // Interpret as fixed-point: value / 1024.0
            v.texcoord0.u = u16 / 1024.0f;
            v.texcoord0.v = v16 / 1024.0f;
        }

        submesh.vertices.push_back(v);
    }

    // Decode indices
    uint32_t actualVertCount = static_cast<uint32_t>(submesh.vertices.size());
    if (!m_data.gpu_index_data.empty()) {
        uint32_t indexCount = static_cast<uint32_t>(m_data.gpu_index_data.size() / 2);
        submesh.indices.reserve(indexCount);
        for (uint32_t i = 0; i < indexCount; i++) {
            uint16_t idx;
            std::memcpy(&idx, m_data.gpu_index_data.data() + i * 2, 2);
            // Only include indices that reference valid vertices
            if (idx < actualVertCount) {
                submesh.indices.push_back(static_cast<uint32_t>(idx));
            }
        }
    } else {
        // No index buffer: generate sequential indices (triangle list)
        uint32_t triCount = actualVertCount / 3;
        submesh.indices.reserve(triCount * 3);
        for (uint32_t i = 0; i < triCount * 3; i++) {
            submesh.indices.push_back(i);
        }
    }

    submesh.bounding_min = mesh->bounding_min;
    submesh.bounding_max = mesh->bounding_max;

    mesh->submeshes.push_back(std::move(submesh));
    m_data.meshes.push_back(std::move(mesh));

    return true;
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

bool WorldChunk::exportOBJ(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to create OBJ file: " << path << "\n";
        return false;
    }

    file << "# OpenSaints Chunk Export\n";
    file << "# Chunk: " << m_data.name << "\n";
    file << "# Bounds: (" << m_data.bounds_min[0] << "," << m_data.bounds_min[1]
         << "," << m_data.bounds_min[2] << ") - ("
         << m_data.bounds_max[0] << "," << m_data.bounds_max[1]
         << "," << m_data.bounds_max[2] << ")\n";
    file << "# Textures: " << m_data.textures.size() << "\n\n";

    uint32_t vertexOffset = 1;

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
