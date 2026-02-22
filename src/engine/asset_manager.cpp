#include "asset_manager.h"
#include "vfs.h"
#include "../formats/vpp.h"
#include "../formats/peg.h"
#include "../formats/preload_table.h"
#include "../formats/xtbl.h"
#include "../formats/mesh.h"
#include <iostream>
#include <algorithm>

namespace opensaints {

AssetManager::AssetManager() = default;

AssetManager::~AssetManager() {
    shutdown();
}

bool AssetManager::initialize(std::shared_ptr<VirtualFileSystem> vfs) {
    if (!vfs) {
        std::cerr << "AssetManager: VFS is null\n";
        return false;
    }

    m_vfs = vfs;
    m_memoryUsage = 0;

    std::cout << "AssetManager initialized with " << m_vfs->getTotalFileCount() << " files\n";
    return true;
}

void AssetManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_textures.clear();
    m_meshes.clear();
    m_xtables.clear();
    m_pegArchives.clear();
    m_preloadTable.reset();
    m_preloadAnimTable.reset();
    m_memoryUsage = 0;

    std::cout << "AssetManager shutdown\n";
}

bool AssetManager::loadPreloadTables() {
    if (!m_vfs) {
        return false;
    }

    // Try to load preload.tbl
    auto preloadResult = m_vfs->read("preload.tbl");
    if (preloadResult.success) {
        m_preloadTable = std::make_shared<PreloadTable>();
        if (m_preloadTable->loadFromMemory(preloadResult.data.data(), preloadResult.data.size())) {
            std::cout << "Loaded preload table: " << m_preloadTable->count() << " entries\n";
        }
    }

    // Try to load preload_anim.tbl
    auto animResult = m_vfs->read("preload_anim.tbl");
    if (animResult.success) {
        m_preloadAnimTable = std::make_shared<PreloadTable>();
        if (m_preloadAnimTable->loadFromMemory(animResult.data.data(), animResult.data.size())) {
            std::cout << "Loaded animation preload table: " << m_preloadAnimTable->count() << " entries\n";
        }
    }

    return m_preloadTable != nullptr;
}

AssetHandle<TextureAsset> AssetManager::loadTexture(const std::string& name) {
    std::string normalized = normalizeName(name);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_textures.find(normalized);
        if (it != m_textures.end()) {
            return AssetHandle<TextureAsset>(it->second);
        }
    }

    auto asset = loadTextureInternal(normalized);
    if (asset) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_textures[normalized] = asset;
    }

    return AssetHandle<TextureAsset>(asset);
}

AssetHandle<MeshAsset> AssetManager::loadMesh(const std::string& name) {
    std::string normalized = normalizeName(name);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_meshes.find(normalized);
        if (it != m_meshes.end()) {
            return AssetHandle<MeshAsset>(it->second);
        }
    }

    auto asset = loadMeshInternal(normalized);
    if (asset) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_meshes[normalized] = asset;
    }

    return AssetHandle<MeshAsset>(asset);
}

AssetHandle<XTableAsset> AssetManager::loadXTable(const std::string& name) {
    std::string normalized = normalizeName(name);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_xtables.find(normalized);
        if (it != m_xtables.end()) {
            return AssetHandle<XTableAsset>(it->second);
        }
    }

    auto asset = loadXTableInternal(normalized);
    if (asset) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_xtables[normalized] = asset;
    }

    return AssetHandle<XTableAsset>(asset);
}

std::future<AssetHandle<TextureAsset>> AssetManager::loadTextureAsync(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        return loadTexture(name);
    });
}

std::future<AssetHandle<MeshAsset>> AssetManager::loadMeshAsync(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        return loadMesh(name);
    });
}

bool AssetManager::isLoaded(const std::string& name) const {
    std::string normalized = normalizeName(name);
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_textures.find(normalized) != m_textures.end()) return true;
    if (m_meshes.find(normalized) != m_meshes.end()) return true;
    if (m_xtables.find(normalized) != m_xtables.end()) return true;

    return false;
}

void AssetManager::unload(const std::string& name) {
    std::string normalized = normalizeName(name);
    std::lock_guard<std::mutex> lock(m_mutex);

    auto texIt = m_textures.find(normalized);
    if (texIt != m_textures.end()) {
        m_memoryUsage -= texIt->second->memoryUsage();
        m_textures.erase(texIt);
        return;
    }

    auto meshIt = m_meshes.find(normalized);
    if (meshIt != m_meshes.end()) {
        m_memoryUsage -= meshIt->second->memoryUsage();
        m_meshes.erase(meshIt);
        return;
    }

    auto xtblIt = m_xtables.find(normalized);
    if (xtblIt != m_xtables.end()) {
        m_memoryUsage -= xtblIt->second->memoryUsage();
        m_xtables.erase(xtblIt);
        return;
    }
}

void AssetManager::collectGarbage() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove textures with no references
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (it->second->refCount() <= 0) {
            m_memoryUsage -= it->second->memoryUsage();
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }

    // Remove meshes with no references
    for (auto it = m_meshes.begin(); it != m_meshes.end();) {
        if (it->second->refCount() <= 0) {
            m_memoryUsage -= it->second->memoryUsage();
            it = m_meshes.erase(it);
        } else {
            ++it;
        }
    }

    // Remove xtables with no references
    for (auto it = m_xtables.begin(); it != m_xtables.end();) {
        if (it->second->refCount() <= 0) {
            m_memoryUsage -= it->second->memoryUsage();
            it = m_xtables.erase(it);
        } else {
            ++it;
        }
    }
}

void AssetManager::trimToMemoryBudget() {
    collectGarbage();

    // If still over budget, unload least recently used assets
    // For now, just log a warning
    if (m_memoryUsage > m_memoryBudget) {
        std::cerr << "Warning: Memory usage (" << (m_memoryUsage / 1024 / 1024)
                  << "MB) exceeds budget (" << (m_memoryBudget / 1024 / 1024) << "MB)\n";
    }
}

AssetManager::Stats AssetManager::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    Stats stats;
    stats.textureCount = m_textures.size();
    stats.meshCount = m_meshes.size();
    stats.xtableCount = m_xtables.size();
    stats.totalAssets = stats.textureCount + stats.meshCount + stats.xtableCount;

    stats.loadedAssets = 0;
    for (const auto& [name, tex] : m_textures) {
        if (tex->state() == AssetState::Loaded) ++stats.loadedAssets;
    }
    for (const auto& [name, mesh] : m_meshes) {
        if (mesh->state() == AssetState::Loaded) ++stats.loadedAssets;
    }
    for (const auto& [name, xtbl] : m_xtables) {
        if (xtbl->state() == AssetState::Loaded) ++stats.loadedAssets;
    }

    stats.memoryUsage = m_memoryUsage;
    stats.memoryBudget = m_memoryBudget;

    return stats;
}

void AssetManager::processHotReloads() {
    if (!m_hotReloadEnabled || !m_vfs) {
        return;
    }

    // Hot reload implementation would check file timestamps
    // and reload modified assets
}

std::shared_ptr<TextureAsset> AssetManager::loadTextureInternal(const std::string& name) {
    auto asset = std::make_shared<TextureAsset>();
    asset->m_name = name;
    asset->m_state = AssetState::Loading;

    // Find the PEG archive containing this texture
    auto pegArchive = findPegArchive(name);
    if (!pegArchive) {
        std::cerr << "Could not find texture: " << name << "\n";
        asset->m_state = AssetState::Failed;
        return asset;
    }

    // Extract RGBA data
    auto rgba = pegArchive->extractRGBA(name);
    if (rgba.empty()) {
        std::cerr << "Failed to extract texture: " << name << "\n";
        asset->m_state = AssetState::Failed;
        return asset;
    }

    // Find texture info
    const auto* texInfo = pegArchive->findTexture(name);
    if (texInfo) {
        asset->m_width = texInfo->width;
        asset->m_height = texInfo->height;
        asset->m_hasAlpha = texInfo->format == PegFormat::DXT3 ||
                           texInfo->format == PegFormat::DXT5 ||
                           texInfo->format == PegFormat::A8R8G8B8 ||
                           texInfo->format == PegFormat::A1R5G5B5 ||
                           texInfo->format == PegFormat::A4R4G4B4;
    }

    asset->m_data = std::move(rgba);
    asset->m_memoryUsage = asset->m_data.size();
    asset->m_state = AssetState::Loaded;

    m_memoryUsage += asset->m_memoryUsage;

    return asset;
}

std::shared_ptr<MeshAsset> AssetManager::loadMeshInternal(const std::string& name) {
    auto asset = std::make_shared<MeshAsset>();
    asset->m_name = name;
    asset->m_state = AssetState::Loading;

    if (!m_vfs) {
        asset->m_state = AssetState::Failed;
        return asset;
    }

    // Try to find and load the mesh file
    std::string cpuPath = name;
    if (cpuPath.find(".cmesh_pc") == std::string::npos &&
        cpuPath.find(".smesh_pc") == std::string::npos) {
        // Try character mesh first, then static mesh
        if (m_vfs->exists(name + ".cmesh_pc")) {
            cpuPath = name + ".cmesh_pc";
        } else if (m_vfs->exists(name + ".smesh_pc")) {
            cpuPath = name + ".smesh_pc";
        }
    }

    // Read CPU file
    auto cpuResult = m_vfs->read(cpuPath);
    if (!cpuResult.success) {
        std::cerr << "Could not find mesh: " << name << "\n";
        asset->m_state = AssetState::Failed;
        return asset;
    }

    // Read GPU file
    std::string gpuPath = cpuPath;
    if (gpuPath.find(".cmesh_pc") != std::string::npos) {
        gpuPath.replace(gpuPath.find(".cmesh_pc"), 9, ".gcmesh_pc");
        asset->m_skinned = true;
    } else if (gpuPath.find(".smesh_pc") != std::string::npos) {
        gpuPath.replace(gpuPath.find(".smesh_pc"), 9, ".gsmesh_pc");
        asset->m_skinned = false;
    }

    auto gpuResult = m_vfs->read(gpuPath);
    if (!gpuResult.success) {
        std::cerr << "Could not find GPU mesh: " << gpuPath << "\n";
        asset->m_state = AssetState::Failed;
        return asset;
    }

    // Parse mesh data from memory buffers
    asset->m_meshData = std::make_shared<MeshData>();

    if (asset->m_skinned) {
        CharacterMesh cmesh;
        if (cmesh.openFromMemory(cpuResult.data.data(), cpuResult.data.size(),
                                  gpuResult.data.data(), gpuResult.data.size(), name)) {
            *asset->m_meshData = cmesh.data();
        } else {
            std::cerr << "Failed to parse character mesh: " << name << "\n";
            asset->m_state = AssetState::Failed;
            return asset;
        }
    } else {
        StaticMesh smesh;
        if (smesh.openFromMemory(cpuResult.data.data(), cpuResult.data.size(),
                                  gpuResult.data.data(), gpuResult.data.size(), name)) {
            *asset->m_meshData = smesh.data();
        } else {
            std::cerr << "Failed to parse static mesh: " << name << "\n";
            asset->m_state = AssetState::Failed;
            return asset;
        }
    }

    asset->m_memoryUsage = cpuResult.data.size() + gpuResult.data.size();
    asset->m_state = AssetState::Loaded;

    m_memoryUsage += asset->m_memoryUsage;

    return asset;
}

std::shared_ptr<XTableAsset> AssetManager::loadXTableInternal(const std::string& name) {
    auto asset = std::make_shared<XTableAsset>();
    asset->m_name = name;
    asset->m_state = AssetState::Loading;

    if (!m_vfs) {
        asset->m_state = AssetState::Failed;
        return asset;
    }

    // Add .xtbl extension if not present
    std::string path = name;
    if (path.find(".xtbl") == std::string::npos) {
        path += ".xtbl";
    }

    auto result = m_vfs->read(path);
    if (!result.success) {
        std::cerr << "Could not find XTBL: " << path << "\n";
        asset->m_state = AssetState::Failed;
        return asset;
    }

    asset->m_document = std::make_shared<XtblDocument>();
    if (!asset->m_document->loadFromMemory(result.data.data(), result.data.size())) {
        std::cerr << "Failed to parse XTBL: " << path << "\n";
        asset->m_state = AssetState::Failed;
        return asset;
    }

    asset->m_memoryUsage = result.data.size();
    asset->m_state = AssetState::Loaded;

    m_memoryUsage += asset->m_memoryUsage;

    return asset;
}

std::shared_ptr<PegArchive> AssetManager::findPegArchive(const std::string& textureName) {
    // Check already loaded PEG archives
    for (auto& [path, peg] : m_pegArchives) {
        if (peg->findTexture(textureName)) {
            return peg;
        }
    }

    // Search for PEG files containing this texture
    if (!m_vfs) {
        return nullptr;
    }

    auto pegFiles = m_vfs->listByExtension("cpeg_pc");
    for (const auto& pegPath : pegFiles) {
        // Check if already loaded
        if (m_pegArchives.find(pegPath) != m_pegArchives.end()) {
            continue;
        }

        // Load CPU header from VFS
        auto cpuResult = m_vfs->read(pegPath);
        if (!cpuResult.success) {
            continue;
        }

        // Find GPU data file (.cpeg_pc -> .gpeg_pc, .cvbm_pc -> .gvbm_pc)
        std::string gpuPath = pegPath;
        auto cpegPos = gpuPath.find(".cpeg_pc");
        auto cvbmPos = gpuPath.find(".cvbm_pc");
        if (cpegPos != std::string::npos) {
            gpuPath.replace(cpegPos, 8, ".gpeg_pc");
        } else if (cvbmPos != std::string::npos) {
            gpuPath.replace(cvbmPos, 8, ".gvbm_pc");
        } else {
            continue;
        }

        auto gpuResult = m_vfs->read(gpuPath);
        if (!gpuResult.success) {
            continue;
        }

        // Parse PEG from memory buffers
        auto peg = std::make_shared<PegArchive>();
        if (!peg->openFromMemory(cpuResult.data.data(), cpuResult.data.size(),
                                  gpuResult.data.data(), gpuResult.data.size())) {
            continue;
        }

        // Cache the loaded archive
        m_pegArchives[pegPath] = peg;

        // Check if this archive contains the texture we're looking for
        if (peg->findTexture(textureName)) {
            return peg;
        }
    }

    return nullptr;
}

std::string AssetManager::normalizeName(const std::string& name) {
    std::string result = name;

    // Convert backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Convert to lowercase
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);

    // Remove leading slashes
    while (!result.empty() && result[0] == '/') {
        result = result.substr(1);
    }

    return result;
}

} // namespace opensaints
