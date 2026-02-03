#pragma once
// Virtual Filesystem for OpenSaints
// Provides unified access to game assets from VPP archives
// Supports mounting multiple archives with priority ordering

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <optional>
#include <functional>

namespace opensaints {

// Forward declarations
class VppArchive;

// File information in the VFS
struct VfsFileInfo {
    std::string path;            // Virtual path (e.g., "textures/player.cpeg_pc")
    std::string source_archive;  // Source VPP archive name
    size_t size;                 // File size in bytes
    size_t offset;               // Offset in source archive
    int priority;                // Mount priority (higher = preferred)
};

// Mount point configuration
struct VfsMountPoint {
    std::string archive_path;    // Path to VPP file
    std::string mount_path;      // Virtual mount point (e.g., "/", "/patch")
    int priority;                // Priority for file resolution
    bool loaded;                 // Is the archive currently loaded?
};

// File read result
struct VfsReadResult {
    std::vector<uint8_t> data;
    bool success;
    std::string error;
};

// Virtual Filesystem
class VirtualFileSystem {
public:
    VirtualFileSystem() = default;
    ~VirtualFileSystem() = default;

    // Mount a VPP archive at a virtual path
    bool mount(const std::filesystem::path& vppPath,
               const std::string& mountPath = "/",
               int priority = 0);

    // Mount all VPP files in a directory
    bool mountDirectory(const std::filesystem::path& directory,
                        const std::string& mountPath = "/",
                        int basePriority = 0);

    // Unmount an archive
    bool unmount(const std::filesystem::path& vppPath);

    // Unmount all archives
    void unmountAll();

    // Check if a file exists
    bool exists(const std::string& virtualPath) const;

    // Get file information
    std::optional<VfsFileInfo> getFileInfo(const std::string& virtualPath) const;

    // Read a file into memory
    VfsReadResult read(const std::string& virtualPath);

    // Read a file as string
    std::optional<std::string> readString(const std::string& virtualPath);

    // List files matching a pattern (glob-style)
    std::vector<std::string> listFiles(const std::string& pattern = "*") const;

    // List files in a virtual directory
    std::vector<std::string> listDirectory(const std::string& virtualDir) const;

    // List files with a specific extension
    std::vector<std::string> listByExtension(const std::string& extension) const;

    // Get all mount points
    const std::vector<VfsMountPoint>& getMountPoints() const { return m_mountPoints; }

    // Get statistics
    size_t getTotalFileCount() const { return m_files.size(); }
    size_t getMountedArchiveCount() const { return m_archives.size(); }

    // Iterate over all files
    void forEachFile(std::function<void(const VfsFileInfo&)> callback) const;

private:
    // Mount points ordered by priority
    std::vector<VfsMountPoint> m_mountPoints;

    // Loaded VPP archives
    std::unordered_map<std::string, std::shared_ptr<VppArchive>> m_archives;

    // File index (virtual path -> file info)
    std::unordered_map<std::string, VfsFileInfo> m_files;

    // Rebuild file index after mount/unmount
    void rebuildIndex();

    // Normalize a virtual path
    static std::string normalizePath(const std::string& path);

    // Match a pattern (simple glob)
    static bool matchPattern(const std::string& pattern, const std::string& path);
};

} // namespace opensaints
