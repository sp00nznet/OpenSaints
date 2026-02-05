#include "vfs.h"
#include "../formats/vpp.h"
#include <iostream>
#include <algorithm>
#include <regex>

namespace opensaints {

bool VirtualFileSystem::mount(const std::filesystem::path& vppPath,
                              const std::string& mountPath,
                              int priority) {
    // Check if already mounted
    std::string pathKey = vppPath.string();
    if (m_archives.find(pathKey) != m_archives.end()) {
        std::cerr << "Archive already mounted: " << vppPath << "\n";
        return false;
    }

    // Open the archive
    auto archive = std::make_shared<VppArchive>();
    if (!archive->open(vppPath)) {
        std::cerr << "Failed to open archive: " << vppPath << "\n";
        return false;
    }

    // Add to archives
    m_archives[pathKey] = archive;

    // Add mount point
    VfsMountPoint mount;
    mount.archive_path = pathKey;
    mount.mount_path = normalizePath(mountPath);
    mount.priority = priority;
    mount.loaded = true;
    m_mountPoints.push_back(mount);

    // Sort mount points by priority (higher first)
    std::sort(m_mountPoints.begin(), m_mountPoints.end(),
              [](const VfsMountPoint& a, const VfsMountPoint& b) {
                  return a.priority > b.priority;
              });

    // Rebuild the file index
    rebuildIndex();

    std::cout << "Mounted: " << vppPath.filename() << " at " << mountPath
              << " (priority " << priority << ", " << archive->fileCount() << " files)\n";

    return true;
}

bool VirtualFileSystem::mountDirectory(const std::filesystem::path& directory,
                                       const std::string& mountPath,
                                       int basePriority) {
    if (!std::filesystem::exists(directory)) {
        std::cerr << "Directory does not exist: " << directory << "\n";
        return false;
    }

    int priority = basePriority;
    int mountedCount = 0;

    // Define priority order for known archives
    static const std::vector<std::string> priorityOrder = {
        "patch",     // Patches override everything
        "common",    // Common assets
        "meshes",
        "textures",
        "pegs",
        "anims",
        "audio",
        "music1", "music2", "music3", "music4",
        "chunks1", "chunks2", "chunks3", "chunks4",
        "cutscenes",
        "city_load"
    };

    // Collect VPP files
    std::vector<std::filesystem::path> vppFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".vpp_pc") {
                vppFiles.push_back(entry.path());
            }
        }
    }

    // Sort by priority order
    const auto& prioOrder = priorityOrder;  // Local reference for lambda capture
    std::sort(vppFiles.begin(), vppFiles.end(),
              [&prioOrder](const std::filesystem::path& a, const std::filesystem::path& b) {
                  std::string nameA = a.stem().string();
                  std::string nameB = b.stem().string();
                  std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
                  std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);

                  auto posA = std::find(prioOrder.begin(), prioOrder.end(), nameA);
                  auto posB = std::find(prioOrder.begin(), prioOrder.end(), nameB);

                  int indexA = (posA != prioOrder.end()) ?
                               std::distance(prioOrder.begin(), posA) :
                               static_cast<int>(prioOrder.size());
                  int indexB = (posB != prioOrder.end()) ?
                               std::distance(prioOrder.begin(), posB) :
                               static_cast<int>(prioOrder.size());

                  return indexA < indexB;
              });

    // Mount each archive
    for (const auto& vppFile : vppFiles) {
        if (mount(vppFile, mountPath, basePriority + priority)) {
            ++mountedCount;
            --priority; // Lower priority for later archives
        }
    }

    std::cout << "Mounted " << mountedCount << " archives from: " << directory << "\n";
    return mountedCount > 0;
}

bool VirtualFileSystem::unmount(const std::filesystem::path& vppPath) {
    std::string pathKey = vppPath.string();

    auto archiveIt = m_archives.find(pathKey);
    if (archiveIt == m_archives.end()) {
        return false;
    }

    // Remove archive
    m_archives.erase(archiveIt);

    // Remove mount point
    m_mountPoints.erase(
        std::remove_if(m_mountPoints.begin(), m_mountPoints.end(),
                       [&pathKey](const VfsMountPoint& mp) {
                           return mp.archive_path == pathKey;
                       }),
        m_mountPoints.end());

    // Rebuild index
    rebuildIndex();

    std::cout << "Unmounted: " << vppPath.filename() << "\n";
    return true;
}

void VirtualFileSystem::unmountAll() {
    m_archives.clear();
    m_mountPoints.clear();
    m_files.clear();

    std::cout << "All archives unmounted\n";
}

bool VirtualFileSystem::exists(const std::string& virtualPath) const {
    std::string normalized = normalizePath(virtualPath);
    return m_files.find(normalized) != m_files.end();
}

std::optional<VfsFileInfo> VirtualFileSystem::getFileInfo(const std::string& virtualPath) const {
    std::string normalized = normalizePath(virtualPath);
    auto it = m_files.find(normalized);
    if (it != m_files.end()) {
        return it->second;
    }
    return std::nullopt;
}

VfsReadResult VirtualFileSystem::read(const std::string& virtualPath) {
    VfsReadResult result;
    result.success = false;

    std::string normalized = normalizePath(virtualPath);
    auto fileIt = m_files.find(normalized);
    if (fileIt == m_files.end()) {
        result.error = "File not found: " + virtualPath;
        return result;
    }

    const auto& fileInfo = fileIt->second;
    auto archiveIt = m_archives.find(fileInfo.source_archive);
    if (archiveIt == m_archives.end()) {
        result.error = "Source archive not loaded: " + fileInfo.source_archive;
        return result;
    }

    // Find file index in archive
    const auto& archive = archiveIt->second;
    const auto& files = archive->files();

    for (size_t i = 0; i < files.size(); ++i) {
        const auto& vppFile = files[i];
        std::string fullName = vppFile.filename;
        if (!vppFile.extension.empty()) {
            fullName += "." + vppFile.extension;
        }

        if (normalizePath(fullName) == normalized ||
            normalizePath(fileInfo.path) == normalized) {
            result.data = archive->extract(i);
            result.success = !result.data.empty() || fileInfo.size == 0;
            if (!result.success) {
                result.error = "Failed to extract file from archive";
            }
            return result;
        }
    }

    result.error = "File not found in archive";
    return result;
}

std::optional<std::string> VirtualFileSystem::readString(const std::string& virtualPath) {
    auto result = read(virtualPath);
    if (!result.success) {
        return std::nullopt;
    }

    return std::string(result.data.begin(), result.data.end());
}

std::vector<std::string> VirtualFileSystem::listFiles(const std::string& pattern) const {
    std::vector<std::string> result;

    for (const auto& [path, info] : m_files) {
        if (pattern == "*" || matchPattern(pattern, path)) {
            result.push_back(path);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> VirtualFileSystem::listDirectory(const std::string& virtualDir) const {
    std::vector<std::string> result;
    std::string normalizedDir = normalizePath(virtualDir);

    // Ensure directory ends with /
    if (!normalizedDir.empty() && normalizedDir.back() != '/') {
        normalizedDir += '/';
    }

    for (const auto& [path, info] : m_files) {
        if (path.find(normalizedDir) == 0) {
            result.push_back(path);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> VirtualFileSystem::listByExtension(const std::string& extension) const {
    std::vector<std::string> result;

    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const auto& [path, info] : m_files) {
        size_t dotPos = path.rfind('.');
        if (dotPos != std::string::npos) {
            std::string fileExt = path.substr(dotPos);
            std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
            if (fileExt == ext) {
                result.push_back(path);
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

void VirtualFileSystem::forEachFile(std::function<void(const VfsFileInfo&)> callback) const {
    for (const auto& [path, info] : m_files) {
        callback(info);
    }
}

void VirtualFileSystem::rebuildIndex() {
    m_files.clear();

    // Process mount points in priority order (highest first)
    for (const auto& mount : m_mountPoints) {
        auto archiveIt = m_archives.find(mount.archive_path);
        if (archiveIt == m_archives.end()) {
            continue;
        }

        const auto& archive = archiveIt->second;
        const auto& files = archive->files();

        for (size_t i = 0; i < files.size(); ++i) {
            const auto& vppFile = files[i];

            // Build full filename
            std::string filename = vppFile.filename;
            if (!vppFile.extension.empty()) {
                filename += "." + vppFile.extension;
            }

            // Build virtual path
            std::string virtualPath = mount.mount_path;
            if (!virtualPath.empty() && virtualPath.back() != '/') {
                virtualPath += '/';
            }
            virtualPath += filename;
            virtualPath = normalizePath(virtualPath);

            // Only add if not already present (higher priority files take precedence)
            if (m_files.find(virtualPath) == m_files.end()) {
                VfsFileInfo info;
                info.path = virtualPath;
                info.source_archive = mount.archive_path;
                info.size = vppFile.data_size;
                info.offset = vppFile.data_offset;
                info.priority = mount.priority;

                m_files[virtualPath] = info;
            }
        }
    }
}

std::string VirtualFileSystem::normalizePath(const std::string& path) {
    std::string result = path;

    // Convert backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove leading slash if present
    while (!result.empty() && result[0] == '/') {
        result = result.substr(1);
    }

    // Convert to lowercase for case-insensitive matching
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);

    return result;
}

bool VirtualFileSystem::matchPattern(const std::string& pattern, const std::string& path) {
    // Convert glob pattern to regex
    std::string regexPattern = pattern;

    // Escape special regex characters
    static const std::string specialChars = ".^$+?()[]{}|\\";
    for (char c : specialChars) {
        size_t pos = 0;
        while ((pos = regexPattern.find(c, pos)) != std::string::npos) {
            regexPattern.insert(pos, "\\");
            pos += 2;
        }
    }

    // Convert glob wildcards to regex
    size_t pos = 0;
    while ((pos = regexPattern.find("\\*", pos)) != std::string::npos) {
        regexPattern.replace(pos, 2, ".*");
        pos += 2;
    }

    pos = 0;
    while ((pos = regexPattern.find("\\?", pos)) != std::string::npos) {
        regexPattern.replace(pos, 2, ".");
        ++pos;
    }

    try {
        std::regex re(regexPattern, std::regex::icase);
        return std::regex_match(path, re);
    } catch (...) {
        return false;
    }
}

} // namespace opensaints
