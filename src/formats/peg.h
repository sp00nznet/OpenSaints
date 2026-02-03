#pragma once
// PEG texture package parser for Saints Row 2
// Handles .cpeg_pc, .gpeg_pc, .cvbm_pc, .gvbm_pc files
// PEG = Packed Exact Geometry (Volition's texture package format)

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace opensaints {

// PEG file signatures
constexpr uint32_t PEG_SIGNATURE = 0x564B4547; // "GEKV" in little-endian
constexpr uint16_t PEG_VERSION = 10;

// Texture format identifiers (D3DFORMAT values)
enum class PegFormat : uint16_t {
    DXT1 = 400,         // BC1 - 4bpp, 1-bit alpha
    DXT3 = 401,         // BC2 - 8bpp, explicit alpha
    DXT5 = 402,         // BC3 - 8bpp, interpolated alpha
    R5G6B5 = 565,       // 16-bit RGB
    A1R5G5B5 = 1555,    // 16-bit ARGB
    A4R4G4B4 = 4444,    // 16-bit ARGB
    A8R8G8B8 = 8888,    // 32-bit ARGB
    V8U8 = 117,         // 16-bit signed bump map
    CxV8U8 = 118,       // Compressed bump map
    A8 = 28,            // 8-bit alpha only
    Unknown = 0
};

// Texture flags
enum class PegFlags : uint16_t {
    None = 0,
    Cubemap = 1 << 0,
    Render_target = 1 << 1,
    System_memory = 1 << 2
};

#pragma pack(push, 1)

// PEG file header
struct PegHeader {
    uint32_t signature;      // Should be PEG_SIGNATURE
    uint16_t version;        // Should be PEG_VERSION (10)
    uint16_t platform;       // Platform identifier (PC = 0)
    uint32_t header_size;    // Size of header section
    uint32_t data_size;      // Total size of texture data
    uint16_t num_textures;   // Number of textures in package
    uint16_t flags;          // Package flags
    uint16_t num_frames;     // Total frames (for animated textures)
    uint16_t reserved;       // Padding
};

// Individual texture entry header
struct PegTextureEntry {
    uint32_t data_offset;    // Offset to texture data (in data file)
    uint16_t width;          // Texture width
    uint16_t height;         // Texture height
    uint16_t format;         // PegFormat value
    uint16_t flags;          // Texture flags
    uint16_t name_offset;    // Offset to name in name table
    uint16_t source_width;   // Original width before padding
    uint16_t source_height;  // Original height before padding
    uint8_t  mip_levels;     // Number of mipmap levels
    uint8_t  frames;         // Number of frames (animated textures)
    uint16_t frame_delay;    // Animation frame delay
    uint32_t data_size;      // Size of texture data
    uint8_t  reserved[8];    // Padding/unknown
};

#pragma pack(pop)

// Parsed texture information
struct PegTexture {
    std::string name;
    uint16_t width;
    uint16_t height;
    uint16_t source_width;    // Original dimensions
    uint16_t source_height;
    PegFormat format;
    uint8_t mip_levels;
    uint8_t frames;
    uint16_t frame_delay;
    size_t data_offset;       // Absolute offset in GPU file
    size_t data_size;

    // Calculate expected data size based on format
    size_t calculateDataSize() const;

    // Get format name string
    const char* formatName() const;

    // Get bits per pixel
    uint32_t bitsPerPixel() const;

    // Is compressed format (DXT)?
    bool isCompressed() const;
};

// PEG archive (CPU header file + GPU data file pair)
class PegArchive {
public:
    PegArchive() = default;
    ~PegArchive() = default;

    // Open a PEG archive (pass either .cpeg_pc or .gpeg_pc, finds the pair)
    bool open(const std::filesystem::path& path);

    // Open with explicit paths
    bool open(const std::filesystem::path& cpuPath,
              const std::filesystem::path& gpuPath);

    // Close the archive
    void close();

    // Get list of textures
    const std::vector<PegTexture>& textures() const { return m_textures; }

    // Find texture by name
    const PegTexture* findTexture(const std::string& name) const;

    // Extract texture raw data
    std::vector<uint8_t> extractRaw(size_t index);
    std::vector<uint8_t> extractRaw(const std::string& name);

    // Extract and decode to RGBA
    std::vector<uint8_t> extractRGBA(size_t index);
    std::vector<uint8_t> extractRGBA(const std::string& name);

    // Check if open
    bool isOpen() const { return m_gpuFile.is_open(); }
    size_t textureCount() const { return m_textures.size(); }

    // Get paths
    const std::filesystem::path& cpuPath() const { return m_cpuPath; }
    const std::filesystem::path& gpuPath() const { return m_gpuPath; }

private:
    std::filesystem::path m_cpuPath;
    std::filesystem::path m_gpuPath;
    std::ifstream m_gpuFile;
    PegHeader m_header{};
    std::vector<PegTexture> m_textures;

    // Decode DXT compressed data to RGBA
    static std::vector<uint8_t> decodeDXT1(const uint8_t* data, uint32_t width, uint32_t height);
    static std::vector<uint8_t> decodeDXT3(const uint8_t* data, uint32_t width, uint32_t height);
    static std::vector<uint8_t> decodeDXT5(const uint8_t* data, uint32_t width, uint32_t height);

    // Decode DXT block
    static void decodeDXT1Block(const uint8_t* block, uint8_t* output, uint32_t stride);
    static void decodeDXT3AlphaBlock(const uint8_t* block, uint8_t* output, uint32_t stride);
    static void decodeDXT5AlphaBlock(const uint8_t* block, uint8_t* output, uint32_t stride);

    // Helper to find paired file
    static std::filesystem::path findPairedFile(const std::filesystem::path& path);
};

} // namespace opensaints
