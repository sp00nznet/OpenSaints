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

// Detect the best vertex stride for a range of vertex data
static int detectVertexStride(const uint8_t* vertexData, uint32_t dataSize,
                              float halfExtX, float halfExtY, float halfExtZ) {
    int bestStride = 20;
    int strideCandidates[] = {20, 24, 28, 32, 36, 40};
    int bestScore = -1;

    for (int stride : strideCandidates) {
        uint32_t vertCount = dataSize / stride;
        if (vertCount == 0 || vertCount > 10000000) continue;

        int inBounds = 0;
        int checkCount = std::min(vertCount, 200u);
        for (int i = 0; i < checkCount; i++) {
            size_t off = static_cast<size_t>(i) * stride;
            if (off + 12 > dataSize) break;

            float x, y, z;
            std::memcpy(&x, vertexData + off, 4);
            std::memcpy(&y, vertexData + off + 4, 4);
            std::memcpy(&z, vertexData + off + 8, 4);

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
    return bestStride;
}

// Decode vertices from a vertex buffer region into a Submesh
static void decodeVertexRange(const uint8_t* vbData, uint32_t vbSize,
                              uint32_t vertStart, uint32_t vertCount, int stride,
                              float cx, float cy, float cz,
                              Submesh& submesh) {
    uint32_t maxVerts = vbSize / stride;
    uint32_t endVert = std::min(vertStart + vertCount, maxVerts);

    submesh.vertices.reserve(endVert - vertStart);

    for (uint32_t i = vertStart; i < endVert; i++) {
        size_t off = static_cast<size_t>(i) * stride;
        if (off + 12 > vbSize) break;

        Vertex v;
        float lx, ly, lz;
        std::memcpy(&lx, vbData + off, 4);
        std::memcpy(&ly, vbData + off + 4, 4);
        std::memcpy(&lz, vbData + off + 8, 4);

        if (!std::isfinite(lx) || !std::isfinite(ly) || !std::isfinite(lz)) {
            // Push a zero vertex to keep index mapping intact
            submesh.vertices.push_back(v);
            continue;
        }

        v.position.x = lx + cx;
        v.position.y = ly + cy;
        v.position.z = lz + cz;

        if (stride >= 16 && off + 16 <= vbSize) {
            uint32_t packedNormal;
            std::memcpy(&packedNormal, vbData + off + 12, 4);
            WorldChunk::decodePackedNormal(packedNormal, v.normal.x, v.normal.y, v.normal.z);
        }

        if (stride >= 20 && off + 20 <= vbSize) {
            uint16_t u16, v16;
            std::memcpy(&u16, vbData + off + 16, 2);
            std::memcpy(&v16, vbData + off + 18, 2);
            v.texcoord0.u = u16 / 1024.0f;
            v.texcoord0.v = v16 / 1024.0f;
        }

        submesh.vertices.push_back(v);
    }
}

// Decode index range, rebasing indices relative to a vertex start offset
static void decodeIndexRange(const uint8_t* ibData, uint32_t ibSize,
                             uint32_t idxStart, uint32_t idxCount,
                             uint32_t vertStart, uint32_t localVertCount,
                             Submesh& submesh) {
    submesh.indices.reserve(idxCount);
    for (uint32_t i = idxStart; i < idxStart + idxCount; i++) {
        if (i * 2 + 2 > ibSize) break;
        uint16_t idx;
        std::memcpy(&idx, ibData + i * 2, 2);
        // Rebase: original index is absolute in the VB, convert to submesh-local
        if (idx >= vertStart && idx < vertStart + localVertCount) {
            submesh.indices.push_back(static_cast<uint32_t>(idx - vertStart));
        }
    }
}

bool WorldChunk::parseGeometry(const std::vector<uint8_t>& gpuData) {
    if (gpuData.empty()) return false;

    uint32_t vbSize = m_data.gpu_vertex_size;
    uint32_t ibSize = m_data.gpu_index_size;

    uint32_t totalExpected = vbSize + ibSize;
    if (totalExpected == 0) {
        return true;
    }

    if (gpuData.size() < totalExpected) {
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

    float halfExtX = (m_data.bounds_max[0] - m_data.bounds_min[0]) * 0.5f + 500.0f;
    float halfExtY = (m_data.bounds_max[1] - m_data.bounds_min[1]) * 0.5f + 500.0f;
    float halfExtZ = (m_data.bounds_max[2] - m_data.bounds_min[2]) * 0.5f + 500.0f;

    int bestStride = detectVertexStride(m_data.gpu_vertex_data.data(), vbSize,
                                        halfExtX, halfExtY, halfExtZ);

    uint32_t totalVertCount = vbSize / bestStride;
    if (totalVertCount == 0) {
        return true;
    }

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

    uint32_t renderItemCount = m_data.render_item_count;
    uint32_t totalIndexCount = ibSize > 0 ? ibSize / 2 : 0;

    // Try to build per-render-item submeshes by splitting geometry evenly
    // Each render item gets a proportional share of vertices and indices
    bool multiSubmeshOk = false;
    if (renderItemCount > 1 && totalVertCount > 0) {
        // Populate render_items with even splits as best-effort
        m_data.render_items.clear();
        m_data.render_items.resize(renderItemCount);

        uint32_t vertsPerItem = totalVertCount / renderItemCount;
        uint32_t indicesPerItem = totalIndexCount > 0 ? totalIndexCount / renderItemCount : 0;

        uint32_t vertOffset = 0;
        uint32_t idxOffset = 0;
        bool allValid = true;

        for (uint32_t ri = 0; ri < renderItemCount; ri++) {
            auto& item = m_data.render_items[ri];
            item.vertex_stride = bestStride;
            item.material_index = (ri < m_data.textures.size()) ? static_cast<int32_t>(ri) : 0;

            // Last item gets the remainder
            uint32_t thisVertCount = (ri == renderItemCount - 1)
                ? (totalVertCount - vertOffset) : vertsPerItem;
            uint32_t thisIdxCount = 0;

            item.vertex_offset = vertOffset * bestStride;
            item.vertex_count = thisVertCount;

            if (totalIndexCount > 0) {
                thisIdxCount = (ri == renderItemCount - 1)
                    ? (totalIndexCount - idxOffset) : indicesPerItem;

                // Find index range that references this vertex range
                // Scan the index buffer to find contiguous run referencing [vertOffset, vertOffset+thisVertCount)
                if (!m_data.gpu_index_data.empty()) {
                    // Find the actual index range by scanning for indices in our vertex range
                    uint32_t scanStart = idxOffset;
                    uint32_t scanCount = 0;

                    for (uint32_t si = scanStart; si < totalIndexCount; si++) {
                        uint16_t idx;
                        std::memcpy(&idx, m_data.gpu_index_data.data() + si * 2, 2);
                        if (idx >= vertOffset && idx < vertOffset + thisVertCount) {
                            scanCount++;
                        } else if (scanCount > 0) {
                            break; // End of contiguous range
                        }
                    }
                    thisIdxCount = scanCount > 0 ? scanCount : thisIdxCount;
                }

                item.index_offset = idxOffset * 2;
                item.index_count = thisIdxCount;
                idxOffset += thisIdxCount;
            }

            vertOffset += thisVertCount;

            Submesh submesh;
            submesh.name = m_data.name + "_sub" + std::to_string(ri);
            submesh.material_index = item.material_index;

            decodeVertexRange(m_data.gpu_vertex_data.data(), vbSize,
                              item.vertex_offset / bestStride, item.vertex_count,
                              bestStride, centerX, centerY, centerZ, submesh);

            if (item.index_count > 0 && !m_data.gpu_index_data.empty()) {
                decodeIndexRange(m_data.gpu_index_data.data(), ibSize,
                                 item.index_offset / 2, item.index_count,
                                 item.vertex_offset / bestStride,
                                 static_cast<uint32_t>(submesh.vertices.size()),
                                 submesh);
            } else {
                // No indices: generate sequential
                for (uint32_t vi = 0; vi < static_cast<uint32_t>(submesh.vertices.size()); vi++) {
                    submesh.indices.push_back(vi);
                }
            }

            if (submesh.vertices.empty()) {
                allValid = false;
                break;
            }

            submesh.bounding_min = mesh->bounding_min;
            submesh.bounding_max = mesh->bounding_max;
            mesh->submeshes.push_back(std::move(submesh));
        }

        if (allValid && !mesh->submeshes.empty()) {
            multiSubmeshOk = true;
        } else {
            mesh->submeshes.clear();
        }
    }

    // Fall back to single submesh
    if (!multiSubmeshOk) {
        Submesh submesh;
        submesh.name = m_data.name + "_geo";
        submesh.material_index = 0;

        decodeVertexRange(m_data.gpu_vertex_data.data(), vbSize,
                          0, totalVertCount, bestStride,
                          centerX, centerY, centerZ, submesh);

        uint32_t actualVertCount = static_cast<uint32_t>(submesh.vertices.size());
        if (!m_data.gpu_index_data.empty()) {
            uint32_t indexCount = totalIndexCount;
            submesh.indices.reserve(indexCount);
            for (uint32_t i = 0; i < indexCount; i++) {
                uint16_t idx;
                std::memcpy(&idx, m_data.gpu_index_data.data() + i * 2, 2);
                if (idx < actualVertCount) {
                    submesh.indices.push_back(static_cast<uint32_t>(idx));
                }
            }
        } else {
            uint32_t triCount = actualVertCount / 3;
            submesh.indices.reserve(triCount * 3);
            for (uint32_t i = 0; i < triCount * 3; i++) {
                submesh.indices.push_back(i);
            }
        }

        submesh.bounding_min = mesh->bounding_min;
        submesh.bounding_max = mesh->bounding_max;
        mesh->submeshes.push_back(std::move(submesh));
    }

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
