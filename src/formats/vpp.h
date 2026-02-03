#pragma once
// VPP_PC file format handler for Saints Row 2
// Based on Kaitai Struct specification and community documentation

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace opensaints {

// VPP file magic: 0x51890ACE (little-endian: CE 0A 89 51)
constexpr uint32_t VPP_MAGIC = 0x51890ACE;
constexpr uint32_t VPP_VERSION = 0x04;
constexpr size_t VPP_ALIGNMENT = 0x800; // 2048 bytes

#pragma pack(push, 1)

struct VppHeader {
    uint32_t magic;           // Should be VPP_MAGIC
    uint32_t version;         // Should be VPP_VERSION (4)
    uint8_t  padding1[0x14C]; // Padding to 0x154
    int32_t  num_files;       // Number of files in archive
    int32_t  container_size;  // Total container size
    int32_t  len_offsets;     // Size of offset table
    int32_t  len_filenames;   // Size of filename section
    int32_t  len_extensions;  // Size of extensions section
    int32_t  reserved[9];     // Unknown/reserved fields
};

struct VppFileEntry {
    uint32_t name_offset;     // Offset into filename section
    uint32_t ext_offset;      // Offset into extension section
    int32_t  unknown;         // Unknown field
    int32_t  data_offset;     // Offset to file data (from data section start)
    int32_t  data_size;       // Size of file data
    int32_t  always_minus1;   // Always -1
    int32_t  always_zero;     // Always 0
};

#pragma pack(pop)

struct VppFileInfo {
    std::string filename;
    std::string extension;
    size_t      data_offset;  // Absolute offset in file
    size_t      data_size;
};

class VppArchive {
public:
    VppArchive() = default;
    ~VppArchive() = default;

    // Open and parse a VPP archive
    bool open(const std::filesystem::path& path);

    // Close the archive
    void close();

    // Get list of files in archive
    const std::vector<VppFileInfo>& files() const { return m_files; }

    // Extract a single file to memory
    std::vector<uint8_t> extract(size_t index);

    // Extract a single file to disk
    bool extractTo(size_t index, const std::filesystem::path& outputPath);

    // Extract all files to a directory
    bool extractAll(const std::filesystem::path& outputDir);

    // Get archive info
    bool isOpen() const { return m_file.is_open(); }
    size_t fileCount() const { return m_files.size(); }
    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
    std::ifstream m_file;
    VppHeader m_header{};
    std::vector<VppFileInfo> m_files;

    // Helper to align to VPP boundary
    static size_t alignTo(size_t offset, size_t alignment = VPP_ALIGNMENT) {
        return ((offset + alignment - 1) / alignment) * alignment;
    }
};

} // namespace opensaints
