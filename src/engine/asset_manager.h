#pragma once
// Asset Manager for OpenSaints
// Central system for loading, caching, and managing game assets
// Supports lazy loading with reference counting and memory budgets

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>
#include <mutex>
#include <atomic>
#include <future>
#include <filesystem>

namespace opensaints {

// Forward declarations
class VirtualFileSystem;
class PegArchive;
class PreloadTable;
class XtblDocument;
struct MeshData;
struct PegTexture;

// Asset types
enum class AssetType {
    Unknown,
    Texture,
    Mesh,
    Animation,
    Audio,
    XTable,
    PreloadTable,
    Chunk,
    Shader,
    Material,
    UI
};

// Asset state
enum class AssetState {
    Unloaded,
    Loading,
    Loaded,
    Failed
};

// Base asset handle
class Asset {
public:
    virtual ~Asset() = default;

    const std::string& name() const { return m_name; }
    AssetType type() const { return m_type; }
    AssetState state() const { return m_state; }
    size_t memoryUsage() const { return m_memoryUsage; }
    int refCount() const { return m_refCount; }

    // Increment/decrement reference count
    void addRef() { ++m_refCount; }
    void release() { --m_refCount; }

protected:
    std::string m_name;
    AssetType m_type = AssetType::Unknown;
    AssetState m_state = AssetState::Unloaded;
    size_t m_memoryUsage = 0;
    std::atomic<int> m_refCount{0};
};

// Texture asset
class TextureAsset : public Asset {
public:
    TextureAsset() { m_type = AssetType::Texture; }

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    const std::vector<uint8_t>& data() const { return m_data; }
    bool hasAlpha() const { return m_hasAlpha; }

    // GPU handle (set by renderer)
    uint32_t gpuHandle = 0;

private:
    friend class AssetManager;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<uint8_t> m_data;
    bool m_hasAlpha = false;
};

// Mesh asset
class MeshAsset : public Asset {
public:
    MeshAsset() { m_type = AssetType::Mesh; }

    const std::shared_ptr<MeshData>& meshData() const { return m_meshData; }
    bool isSkinned() const { return m_skinned; }

    // GPU handles
    uint32_t vertexBufferHandle = 0;
    uint32_t indexBufferHandle = 0;

private:
    friend class AssetManager;
    std::shared_ptr<MeshData> m_meshData;
    bool m_skinned = false;
};

// XTable asset
class XTableAsset : public Asset {
public:
    XTableAsset() { m_type = AssetType::XTable; }

    const std::shared_ptr<XtblDocument>& document() const { return m_document; }

private:
    friend class AssetManager;
    std::shared_ptr<XtblDocument> m_document;
};

// Asset handle (smart pointer with reference counting)
template<typename T>
class AssetHandle {
public:
    AssetHandle() : m_asset(nullptr) {}

    explicit AssetHandle(std::shared_ptr<T> asset) : m_asset(asset) {
        if (m_asset) {
            m_asset->addRef();
        }
    }

    AssetHandle(const AssetHandle& other) : m_asset(other.m_asset) {
        if (m_asset) {
            m_asset->addRef();
        }
    }

    AssetHandle(AssetHandle&& other) noexcept : m_asset(std::move(other.m_asset)) {
        other.m_asset = nullptr;
    }

    ~AssetHandle() {
        if (m_asset) {
            m_asset->release();
        }
    }

    AssetHandle& operator=(const AssetHandle& other) {
        if (this != &other) {
            if (m_asset) {
                m_asset->release();
            }
            m_asset = other.m_asset;
            if (m_asset) {
                m_asset->addRef();
            }
        }
        return *this;
    }

    AssetHandle& operator=(AssetHandle&& other) noexcept {
        if (this != &other) {
            if (m_asset) {
                m_asset->release();
            }
            m_asset = std::move(other.m_asset);
            other.m_asset = nullptr;
        }
        return *this;
    }

    T* get() const { return m_asset.get(); }
    T* operator->() const { return m_asset.get(); }
    T& operator*() const { return *m_asset; }
    explicit operator bool() const { return m_asset != nullptr; }

    bool isLoaded() const {
        return m_asset && m_asset->state() == AssetState::Loaded;
    }

private:
    std::shared_ptr<T> m_asset;
};

// Asset Manager
class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Initialize with virtual filesystem
    bool initialize(std::shared_ptr<VirtualFileSystem> vfs);

    // Shutdown and release all assets
    void shutdown();

    // Load preload tables
    bool loadPreloadTables();

    // Set memory budget (bytes)
    void setMemoryBudget(size_t bytes) { m_memoryBudget = bytes; }
    size_t getMemoryBudget() const { return m_memoryBudget; }
    size_t getMemoryUsage() const { return m_memoryUsage; }

    // Synchronous asset loading
    AssetHandle<TextureAsset> loadTexture(const std::string& name);
    AssetHandle<MeshAsset> loadMesh(const std::string& name);
    AssetHandle<XTableAsset> loadXTable(const std::string& name);

    // Asynchronous asset loading
    std::future<AssetHandle<TextureAsset>> loadTextureAsync(const std::string& name);
    std::future<AssetHandle<MeshAsset>> loadMeshAsync(const std::string& name);

    // Check if asset is loaded
    bool isLoaded(const std::string& name) const;

    // Unload specific asset
    void unload(const std::string& name);

    // Unload unused assets (refCount == 0)
    void collectGarbage();

    // Unload assets to fit within memory budget
    void trimToMemoryBudget();

    // Get asset statistics
    struct Stats {
        size_t totalAssets;
        size_t loadedAssets;
        size_t textureCount;
        size_t meshCount;
        size_t xtableCount;
        size_t memoryUsage;
        size_t memoryBudget;
    };
    Stats getStats() const;

    // Enable/disable hot reloading
    void setHotReloadEnabled(bool enabled) { m_hotReloadEnabled = enabled; }
    bool isHotReloadEnabled() const { return m_hotReloadEnabled; }

    // Check for and process hot reloads
    void processHotReloads();

    // Get the VFS
    std::shared_ptr<VirtualFileSystem> getVFS() const { return m_vfs; }

private:
    std::shared_ptr<VirtualFileSystem> m_vfs;

    // Asset caches
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> m_meshes;
    std::unordered_map<std::string, std::shared_ptr<XTableAsset>> m_xtables;

    // PEG archive cache (texture packages)
    std::unordered_map<std::string, std::shared_ptr<PegArchive>> m_pegArchives;

    // Preload tables
    std::shared_ptr<PreloadTable> m_preloadTable;
    std::shared_ptr<PreloadTable> m_preloadAnimTable;

    // Memory tracking
    std::atomic<size_t> m_memoryUsage{0};
    size_t m_memoryBudget = 512 * 1024 * 1024; // 512MB default

    // Threading
    mutable std::mutex m_mutex;

    // Hot reload
    bool m_hotReloadEnabled = false;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimestamps;

    // Internal loading functions
    std::shared_ptr<TextureAsset> loadTextureInternal(const std::string& name);
    std::shared_ptr<MeshAsset> loadMeshInternal(const std::string& name);
    std::shared_ptr<XTableAsset> loadXTableInternal(const std::string& name);

    // Find the PEG archive containing a texture
    std::shared_ptr<PegArchive> findPegArchive(const std::string& textureName);

    // Normalize asset name for lookups
    static std::string normalizeName(const std::string& name);
};

} // namespace opensaints
