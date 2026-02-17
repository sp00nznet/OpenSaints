#include "streaming.h"
#include "../engine/asset_manager.h"
#include "../engine/vfs.h"
#include "../formats/chunk.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace opensaints {

// WorldVec3 implementation

float WorldVec3::distanceTo(const WorldVec3& other) const {
    return std::sqrt(distanceSquaredTo(other));
}

float WorldVec3::distanceSquaredTo(const WorldVec3& other) const {
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

// WorldBounds implementation

WorldVec3 WorldBounds::center() const {
    return WorldVec3(
        (min.x + max.x) * 0.5f,
        (min.y + max.y) * 0.5f,
        (min.z + max.z) * 0.5f
    );
}

WorldVec3 WorldBounds::size() const {
    return WorldVec3(
        max.x - min.x,
        max.y - min.y,
        max.z - min.z
    );
}

bool WorldBounds::contains(const WorldVec3& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool WorldBounds::intersects(const WorldBounds& other) const {
    return min.x <= other.max.x && max.x >= other.min.x &&
           min.y <= other.max.y && max.y >= other.min.y &&
           min.z <= other.max.z && max.z >= other.min.z;
}

float WorldBounds::distanceToPoint(const WorldVec3& point) const {
    float dx = std::max(std::max(min.x - point.x, point.x - max.x), 0.0f);
    float dy = std::max(std::max(min.y - point.y, point.y - max.y), 0.0f);
    float dz = std::max(std::max(min.z - point.z, point.z - max.z), 0.0f);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// StreamingManager implementation

StreamingManager::StreamingManager() = default;

StreamingManager::~StreamingManager() {
    shutdown();
}

bool StreamingManager::initialize(std::shared_ptr<AssetManager> assets) {
    m_assets = assets;

    if (!m_assets) {
        std::cerr << "StreamingManager: AssetManager is null\n";
        return false;
    }

    std::cout << "StreamingManager initialized\n";
    return true;
}

void StreamingManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Wait for all loading operations
    for (auto& [id, future] : m_loading) {
        if (future.valid()) {
            future.wait();
        }
    }
    m_loading.clear();

    // Unload all chunks
    for (auto& [id, managed] : m_chunks) {
        if (managed.chunk) {
            if (m_unloadedCallback) {
                m_unloadedCallback(id);
            }
            managed.chunk.reset();
        }
        managed.state = ChunkState::Unloaded;
    }

    m_chunks.clear();

    // Clear load queue
    while (!m_loadQueue.empty()) {
        m_loadQueue.pop();
    }

    std::cout << "StreamingManager shutdown\n";
}

void StreamingManager::registerChunkFile(const std::string& filename, const WorldBounds& bounds,
                                         int32_t gridX, int32_t gridY, int32_t gridZ, uint8_t lod) {
    ChunkId id{gridX, gridY, gridZ, lod};

    ManagedChunk managed;
    managed.id = id;
    managed.filename = filename;
    managed.bounds = bounds;
    managed.state = ChunkState::Unloaded;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_chunks[id] = std::move(managed);
}

void StreamingManager::discoverChunks() {
    if (!m_assets) return;

    auto vfs = m_assets->getVFS();
    if (!vfs) return;

    // Find all .chunk_pc files (exclude .g_chunk_pc GPU files)
    auto allFiles = vfs->listByExtension("chunk_pc");

    std::vector<std::string> chunkFiles;
    for (const auto& f : allFiles) {
        // Skip GPU data files
        if (f.find(".g_chunk_pc") != std::string::npos) continue;
        if (f.find(".g_peg_pc") != std::string::npos) continue;
        chunkFiles.push_back(f);
    }

    std::cout << "Discovering chunks: " << chunkFiles.size() << " CPU files found\n";

    int registered = 0;
    int gridIndex = 0;
    for (const auto& filename : chunkFiles) {
        // Quick-parse the header to get real bounds
        auto result = vfs->read(filename);
        if (!result.success || result.data.size() < 0xF0) {
            // Can't read header - skip
            continue;
        }

        // Validate signature
        uint32_t signature = 0;
        std::memcpy(&signature, result.data.data(), 4);
        if (signature != 0xBBCACA12) continue;

        // Read bounds from offset 0xD4 (within ChunkGeometryInfo)
        float boundsMin[3], boundsMax[3];
        std::memcpy(boundsMin, result.data.data() + 0xD4, sizeof(float) * 3);
        std::memcpy(boundsMax, result.data.data() + 0xE0, sizeof(float) * 3);

        // Validate bounds are plausible (finite, non-zero extent)
        bool validBounds = true;
        for (int i = 0; i < 3; i++) {
            if (!std::isfinite(boundsMin[i]) || !std::isfinite(boundsMax[i]) ||
                boundsMax[i] <= boundsMin[i]) {
                validBounds = false;
                break;
            }
        }

        WorldBounds bounds;
        if (validBounds) {
            bounds.min = WorldVec3(boundsMin[0], boundsMin[1], boundsMin[2]);
            bounds.max = WorldVec3(boundsMax[0], boundsMax[1], boundsMax[2]);
        } else {
            // Fallback: assign sequential grid positions
            float chunkSize = 256.0f;
            bounds.min = WorldVec3(gridIndex * chunkSize, 0, 0);
            bounds.max = WorldVec3((gridIndex + 1) * chunkSize, chunkSize, chunkSize);
        }

        // Derive grid coordinates from bounds center
        WorldVec3 center = bounds.center();
        float gridSize = 256.0f;
        int32_t gx = static_cast<int32_t>(std::floor(center.x / gridSize));
        int32_t gy = static_cast<int32_t>(std::floor(center.y / gridSize));
        int32_t gz = static_cast<int32_t>(std::floor(center.z / gridSize));

        registerChunkFile(filename, bounds, gx, gy, gz, 0);
        registered++;
        gridIndex++;
    }

    std::cout << "Registered " << registered << " chunks with real bounds\n";
}

void StreamingManager::update(const WorldVec3& playerPos, const WorldVec3& playerVelocity, float deltaTime) {
    m_playerPos = playerPos;
    m_playerVelocity = playerVelocity;
    m_totalTime += deltaTime;
    m_timeSinceUpdate += deltaTime;

    // Throttle updates
    if (m_timeSinceUpdate < m_config.updateInterval) {
        // Still check for load completion
        checkLoadCompletion();
        return;
    }
    m_timeSinceUpdate = 0;

    // Update chunk priorities and distances
    updateChunkPriorities();

    // Unload distant chunks
    unloadDistantChunks();

    // Process load queue
    processLoadQueue();

    // Check for completed loads
    checkLoadCompletion();

    // Predictive loading based on velocity
    if (m_config.enablePredictiveLoading) {
        predictiveLoad();
    }
}

void StreamingManager::updateChunkPriorities() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, managed] : m_chunks) {
        // Calculate distance to player
        float distance = managed.bounds.distanceToPoint(m_playerPos);
        managed.distanceToPlayer = distance;

        // Determine if visible (simple distance check)
        managed.visible = distance < m_config.loadRadius;

        // Update last access time for loaded chunks
        if (managed.state == ChunkState::Loaded && managed.visible) {
            managed.lastAccessTime = m_totalTime;
        }

        // Determine priority based on distance
        if (managed.bounds.contains(m_playerPos)) {
            managed.priority = LoadPriority::Immediate;
        } else if (distance < m_config.loadRadius * 0.25f) {
            managed.priority = LoadPriority::High;
        } else if (distance < m_config.loadRadius * 0.5f) {
            managed.priority = LoadPriority::Medium;
        } else if (distance < m_config.loadRadius) {
            managed.priority = LoadPriority::Low;
        } else {
            managed.priority = LoadPriority::Preload;
        }

        // Queue for loading if needed and not already loading/loaded
        if (managed.visible && managed.state == ChunkState::Unloaded) {
            m_loadQueue.push({id, managed.priority, distance});
            managed.state = ChunkState::Loading;
        }
    }
}

void StreamingManager::processLoadQueue() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Process load queue up to concurrent limit
    while (!m_loadQueue.empty() &&
           static_cast<int>(m_loading.size()) < m_config.maxConcurrentLoads) {

        LoadRequest request = m_loadQueue.top();
        m_loadQueue.pop();

        auto it = m_chunks.find(request.id);
        if (it == m_chunks.end()) continue;

        ManagedChunk& managed = it->second;

        // Skip if already loading or loaded
        if (managed.state != ChunkState::Loading) continue;

        // Check if already in loading map
        if (m_loading.find(request.id) != m_loading.end()) continue;

        // Start async load
        std::string filename = managed.filename;
        m_loading[request.id] = std::async(std::launch::async, [this, filename]() {
            return loadChunkSync(filename);
        });

        if (m_debugMode) {
            std::cout << "Loading chunk: " << filename << "\n";
        }
    }
}

void StreamingManager::checkLoadCompletion() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ChunkId> completed;

    for (auto& [id, future] : m_loading) {
        if (future.valid() &&
            future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            completed.push_back(id);
        }
    }

    for (const auto& id : completed) {
        auto& future = m_loading[id];
        auto chunk = future.get();
        m_loading.erase(id);

        auto it = m_chunks.find(id);
        if (it == m_chunks.end()) continue;

        ManagedChunk& managed = it->second;

        if (chunk) {
            managed.chunk = chunk;
            managed.state = ChunkState::Loaded;
            managed.lastAccessTime = m_totalTime;

            if (m_loadedCallback) {
                m_loadedCallback(id, chunk);
            }

            if (m_debugMode) {
                std::cout << "Chunk loaded: " << managed.filename << "\n";
            }
        } else {
            managed.state = ChunkState::Failed;
            std::cerr << "Failed to load chunk: " << managed.filename << "\n";
        }
    }
}

void StreamingManager::unloadDistantChunks() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Count loaded chunks
    int loadedCount = 0;
    for (const auto& [id, managed] : m_chunks) {
        if (managed.state == ChunkState::Loaded) {
            loadedCount++;
        }
    }

    // Unload chunks beyond unload radius
    std::vector<ChunkId> toUnload;

    for (auto& [id, managed] : m_chunks) {
        if (managed.state != ChunkState::Loaded) continue;

        bool shouldUnload = false;

        // Unload if too far
        if (managed.distanceToPlayer > m_config.unloadRadius) {
            shouldUnload = true;
        }

        // Unload if over memory limit (LRU)
        if (loadedCount > m_config.maxChunksLoaded) {
            float age = m_totalTime - managed.lastAccessTime;
            if (age > 5.0f && managed.distanceToPlayer > m_config.loadRadius) {
                shouldUnload = true;
            }
        }

        if (shouldUnload) {
            toUnload.push_back(id);
        }
    }

    for (const auto& id : toUnload) {
        unloadChunk(id);
    }
}

void StreamingManager::predictiveLoad() {
    // Predict where player will be in 2 seconds
    WorldVec3 predictedPos(
        m_playerPos.x + m_playerVelocity.x * 2.0f,
        m_playerPos.y + m_playerVelocity.y * 2.0f,
        m_playerPos.z + m_playerVelocity.z * 2.0f
    );

    // Find chunks near predicted position
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [id, managed] : m_chunks) {
        if (managed.state != ChunkState::Unloaded) continue;

        float predictedDistance = managed.bounds.distanceToPoint(predictedPos);
        if (predictedDistance < m_config.loadRadius * 0.5f) {
            managed.priority = LoadPriority::Preload;
            m_loadQueue.push({id, LoadPriority::Preload, predictedDistance});
            managed.state = ChunkState::Loading;
        }
    }
}

void StreamingManager::loadChunk(const ChunkId& id, LoadPriority priority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_chunks.find(id);
    if (it == m_chunks.end()) return;

    ManagedChunk& managed = it->second;
    if (managed.state != ChunkState::Unloaded) return;

    managed.priority = priority;
    m_loadQueue.push({id, priority, managed.distanceToPlayer});
    managed.state = ChunkState::Loading;
}

void StreamingManager::unloadChunk(const ChunkId& id) {
    auto it = m_chunks.find(id);
    if (it == m_chunks.end()) return;

    ManagedChunk& managed = it->second;
    if (managed.state != ChunkState::Loaded) return;

    if (m_unloadedCallback) {
        m_unloadedCallback(id);
    }

    managed.chunk.reset();
    managed.state = ChunkState::Unloaded;

    if (m_debugMode) {
        std::cout << "Chunk unloaded: " << managed.filename << "\n";
    }
}

std::shared_ptr<WorldChunk> StreamingManager::getChunk(const ChunkId& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_chunks.find(id);
    if (it != m_chunks.end() && it->second.state == ChunkState::Loaded) {
        it->second.lastAccessTime = m_totalTime;
        return it->second.chunk;
    }
    return nullptr;
}

std::vector<std::shared_ptr<WorldChunk>> StreamingManager::getVisibleChunks() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<WorldChunk>> visible;
    for (const auto& [id, managed] : m_chunks) {
        if (managed.state == ChunkState::Loaded && managed.visible && managed.chunk) {
            visible.push_back(managed.chunk);
        }
    }
    return visible;
}

StreamingStats StreamingManager::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    StreamingStats stats = {};

    for (const auto& [id, managed] : m_chunks) {
        switch (managed.state) {
            case ChunkState::Loaded:
                stats.chunksLoaded++;
                if (managed.visible) stats.chunksVisible++;
                break;
            case ChunkState::Loading:
                stats.chunksLoading++;
                break;
            default:
                break;
        }
    }

    stats.chunksQueued = m_loadQueue.size();

    return stats;
}

std::vector<std::pair<WorldBounds, ChunkState>> StreamingManager::getDebugChunkBounds() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::pair<WorldBounds, ChunkState>> result;
    for (const auto& [id, managed] : m_chunks) {
        result.push_back({managed.bounds, managed.state});
    }
    return result;
}

ChunkId StreamingManager::worldToChunkId(const WorldVec3& pos, uint8_t lod) const {
    float chunkSize = 256.0f; // Base chunk size
    return ChunkId{
        static_cast<int32_t>(std::floor(pos.x / chunkSize)),
        static_cast<int32_t>(std::floor(pos.y / chunkSize)),
        static_cast<int32_t>(std::floor(pos.z / chunkSize)),
        lod
    };
}

WorldBounds StreamingManager::chunkIdToBounds(const ChunkId& id) const {
    float chunkSize = 256.0f;
    WorldBounds bounds;
    bounds.min = WorldVec3(id.x * chunkSize, id.y * chunkSize, id.z * chunkSize);
    bounds.max = WorldVec3((id.x + 1) * chunkSize, (id.y + 1) * chunkSize, (id.z + 1) * chunkSize);
    return bounds;
}

std::shared_ptr<WorldChunk> StreamingManager::loadChunkSync(const std::string& filename) {
    if (!m_assets) return nullptr;

    auto vfs = m_assets->getVFS();
    if (!vfs) return nullptr;

    // Read CPU file (.chunk_pc)
    auto cpuResult = vfs->read(filename);
    if (!cpuResult.success) {
        std::cerr << "Failed to read chunk CPU file: " << filename << "\n";
        return nullptr;
    }

    // Try to read paired GPU file (.g_chunk_pc)
    std::vector<uint8_t> gpuData;
    std::string gpuFilename = filename;
    // Replace .chunk_pc with .g_chunk_pc
    size_t extPos = gpuFilename.rfind(".chunk_pc");
    if (extPos != std::string::npos) {
        gpuFilename = gpuFilename.substr(0, extPos) + ".g_chunk_pc";
        auto gpuResult = vfs->read(gpuFilename);
        if (gpuResult.success) {
            gpuData = std::move(gpuResult.data);
        }
        // Missing GPU file is OK - some chunks are header-only
    }

    // Extract a clean name from the filename
    std::string name = filename;
    size_t slashPos = name.find_last_of("/\\");
    if (slashPos != std::string::npos) name = name.substr(slashPos + 1);
    size_t dotPos = name.rfind(".chunk_pc");
    if (dotPos != std::string::npos) name = name.substr(0, dotPos);

    // Parse chunk from memory buffers
    auto chunk = std::make_shared<WorldChunk>();
    if (!chunk->openFromMemory(name, cpuResult.data, gpuData)) {
        std::cerr << "Failed to parse chunk: " << filename << "\n";
        return nullptr;
    }

    return chunk;
}

} // namespace opensaints
