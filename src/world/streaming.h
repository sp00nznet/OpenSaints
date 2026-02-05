#pragma once
// World Streaming System for OpenSaints
// Manages loading/unloading of world chunks based on player position

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <future>

namespace opensaints {

// Forward declarations
class AssetManager;
class WorldChunk;
struct ChunkData;

// 3D Vector for world positions
struct WorldVec3 {
    float x, y, z;

    WorldVec3() : x(0), y(0), z(0) {}
    WorldVec3(float x, float y, float z) : x(x), y(y), z(z) {}

    float distanceTo(const WorldVec3& other) const;
    float distanceSquaredTo(const WorldVec3& other) const;
};

// Axis-aligned bounding box
struct WorldBounds {
    WorldVec3 min;
    WorldVec3 max;

    WorldVec3 center() const;
    WorldVec3 size() const;
    bool contains(const WorldVec3& point) const;
    bool intersects(const WorldBounds& other) const;
    float distanceToPoint(const WorldVec3& point) const;
};

// Chunk identifier
struct ChunkId {
    int32_t x, y, z;  // Grid coordinates
    uint8_t lod;      // Level of detail

    bool operator==(const ChunkId& other) const {
        return x == other.x && y == other.y && z == other.z && lod == other.lod;
    }
};

// Hash function for ChunkId
struct ChunkIdHash {
    size_t operator()(const ChunkId& id) const {
        return std::hash<int32_t>()(id.x) ^
               (std::hash<int32_t>()(id.y) << 1) ^
               (std::hash<int32_t>()(id.z) << 2) ^
               (std::hash<uint8_t>()(id.lod) << 3);
    }
};

// Chunk load state
enum class ChunkState {
    Unloaded,
    Loading,
    Loaded,
    Unloading,
    Failed
};

// Chunk load priority
enum class LoadPriority {
    Immediate = 0,  // Must load now (player is here)
    High = 1,       // Load soon (adjacent to player)
    Medium = 2,     // Load when convenient (visible)
    Low = 3,        // Background loading (LOD, distant)
    Preload = 4     // Speculative loading
};

// Managed chunk entry
struct ManagedChunk {
    ChunkId id;
    std::string filename;
    WorldBounds bounds;
    ChunkState state = ChunkState::Unloaded;
    LoadPriority priority = LoadPriority::Low;
    std::shared_ptr<WorldChunk> chunk;
    float lastAccessTime = 0;
    float distanceToPlayer = 0;
    bool visible = false;
};

// Streaming configuration
struct StreamingConfig {
    float loadRadius = 500.0f;        // Distance to load chunks
    float unloadRadius = 600.0f;      // Distance to unload chunks
    float lodTransitionDist = 200.0f; // Distance for LOD transitions
    int maxConcurrentLoads = 4;       // Max simultaneous loads
    int maxChunksLoaded = 100;        // Max chunks in memory
    float updateInterval = 0.1f;      // Seconds between updates
    bool enableLOD = true;
    bool enablePredictiveLoading = true;
};

// Streaming statistics
struct StreamingStats {
    uint32_t chunksLoaded;
    uint32_t chunksLoading;
    uint32_t chunksQueued;
    uint32_t chunksVisible;
    size_t memoryUsage;
    float averageLoadTime;
    uint32_t totalLoadsThisFrame;
    uint32_t totalUnloadsThisFrame;
};

// Callback types
using ChunkLoadedCallback = std::function<void(const ChunkId&, std::shared_ptr<WorldChunk>)>;
using ChunkUnloadedCallback = std::function<void(const ChunkId&)>;

// World Streaming Manager
class StreamingManager {
public:
    StreamingManager();
    ~StreamingManager();

    // Initialize with asset manager
    bool initialize(std::shared_ptr<AssetManager> assets);

    // Shutdown
    void shutdown();

    // Register chunk files from VPP archives
    void registerChunkFile(const std::string& filename, const WorldBounds& bounds,
                          int32_t gridX, int32_t gridY, int32_t gridZ, uint8_t lod = 0);

    // Auto-discover chunks from asset manager
    void discoverChunks();

    // Update streaming based on player position
    void update(const WorldVec3& playerPos, const WorldVec3& playerVelocity, float deltaTime);

    // Force load/unload specific chunk
    void loadChunk(const ChunkId& id, LoadPriority priority = LoadPriority::High);
    void unloadChunk(const ChunkId& id);

    // Get loaded chunk
    std::shared_ptr<WorldChunk> getChunk(const ChunkId& id);

    // Get all visible chunks for rendering
    std::vector<std::shared_ptr<WorldChunk>> getVisibleChunks() const;

    // Configuration
    void setConfig(const StreamingConfig& config) { m_config = config; }
    const StreamingConfig& getConfig() const { return m_config; }

    // Statistics
    StreamingStats getStats() const;

    // Callbacks
    void setChunkLoadedCallback(ChunkLoadedCallback callback) { m_loadedCallback = callback; }
    void setChunkUnloadedCallback(ChunkUnloadedCallback callback) { m_unloadedCallback = callback; }

    // Debug
    void setDebugMode(bool enabled) { m_debugMode = enabled; }
    std::vector<std::pair<WorldBounds, ChunkState>> getDebugChunkBounds() const;

private:
    std::shared_ptr<AssetManager> m_assets;
    StreamingConfig m_config;

    // All known chunks
    std::unordered_map<ChunkId, ManagedChunk, ChunkIdHash> m_chunks;

    // Load queue (priority queue)
    struct LoadRequest {
        ChunkId id;
        LoadPriority priority;
        float distance;

        bool operator<(const LoadRequest& other) const {
            if (priority != other.priority) return priority > other.priority;
            return distance > other.distance;
        }
    };
    std::priority_queue<LoadRequest> m_loadQueue;

    // Currently loading chunks
    std::unordered_map<ChunkId, std::future<std::shared_ptr<WorldChunk>>, ChunkIdHash> m_loading;

    // Player state for prediction
    WorldVec3 m_playerPos;
    WorldVec3 m_playerVelocity;
    float m_timeSinceUpdate = 0;
    float m_totalTime = 0;

    // Callbacks
    ChunkLoadedCallback m_loadedCallback;
    ChunkUnloadedCallback m_unloadedCallback;

    // Threading
    mutable std::mutex m_mutex;

    // Debug
    bool m_debugMode = false;

    // Internal methods
    void updateChunkPriorities();
    void processLoadQueue();
    void checkLoadCompletion();
    void unloadDistantChunks();
    void predictiveLoad();

    ChunkId worldToChunkId(const WorldVec3& pos, uint8_t lod = 0) const;
    WorldBounds chunkIdToBounds(const ChunkId& id) const;

    std::shared_ptr<WorldChunk> loadChunkSync(const std::string& filename);
};

} // namespace opensaints
