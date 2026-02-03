#include "peg.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace opensaints {

// PegTexture implementation

size_t PegTexture::calculateDataSize() const {
    size_t size = 0;
    uint32_t w = width;
    uint32_t h = height;

    for (uint8_t mip = 0; mip < std::max(mip_levels, uint8_t(1)); ++mip) {
        size_t mipSize;
        switch (format) {
            case PegFormat::DXT1:
                // 4x4 blocks, 8 bytes per block
                mipSize = ((w + 3) / 4) * ((h + 3) / 4) * 8;
                break;
            case PegFormat::DXT3:
            case PegFormat::DXT5:
                // 4x4 blocks, 16 bytes per block
                mipSize = ((w + 3) / 4) * ((h + 3) / 4) * 16;
                break;
            case PegFormat::A8R8G8B8:
                mipSize = w * h * 4;
                break;
            case PegFormat::R5G6B5:
            case PegFormat::A1R5G5B5:
            case PegFormat::A4R4G4B4:
            case PegFormat::V8U8:
            case PegFormat::CxV8U8:
                mipSize = w * h * 2;
                break;
            case PegFormat::A8:
                mipSize = w * h;
                break;
            default:
                mipSize = w * h * 4; // Assume 32bpp
                break;
        }
        size += mipSize;

        w = std::max(w / 2, 1u);
        h = std::max(h / 2, 1u);
    }

    return size * std::max(frames, uint8_t(1));
}

const char* PegTexture::formatName() const {
    switch (format) {
        case PegFormat::DXT1: return "DXT1";
        case PegFormat::DXT3: return "DXT3";
        case PegFormat::DXT5: return "DXT5";
        case PegFormat::R5G6B5: return "R5G6B5";
        case PegFormat::A1R5G5B5: return "A1R5G5B5";
        case PegFormat::A4R4G4B4: return "A4R4G4B4";
        case PegFormat::A8R8G8B8: return "A8R8G8B8";
        case PegFormat::V8U8: return "V8U8";
        case PegFormat::CxV8U8: return "CxV8U8";
        case PegFormat::A8: return "A8";
        default: return "Unknown";
    }
}

uint32_t PegTexture::bitsPerPixel() const {
    switch (format) {
        case PegFormat::DXT1: return 4;
        case PegFormat::DXT3:
        case PegFormat::DXT5: return 8;
        case PegFormat::A8R8G8B8: return 32;
        case PegFormat::R5G6B5:
        case PegFormat::A1R5G5B5:
        case PegFormat::A4R4G4B4:
        case PegFormat::V8U8:
        case PegFormat::CxV8U8: return 16;
        case PegFormat::A8: return 8;
        default: return 32;
    }
}

bool PegTexture::isCompressed() const {
    return format == PegFormat::DXT1 ||
           format == PegFormat::DXT3 ||
           format == PegFormat::DXT5;
}

// PegArchive implementation

bool PegArchive::open(const std::filesystem::path& path) {
    auto paired = findPairedFile(path);
    if (paired.empty()) {
        std::cerr << "Could not find paired PEG file for: " << path << "\n";
        return false;
    }

    std::filesystem::path cpuPath, gpuPath;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".cpeg_pc" || ext == ".cvbm_pc") {
        cpuPath = path;
        gpuPath = paired;
    } else {
        cpuPath = paired;
        gpuPath = path;
    }

    return open(cpuPath, gpuPath);
}

bool PegArchive::open(const std::filesystem::path& cpuPath,
                      const std::filesystem::path& gpuPath) {
    close();

    // Open CPU header file
    std::ifstream cpuFile(cpuPath, std::ios::binary);
    if (!cpuFile.is_open()) {
        std::cerr << "Failed to open CPU file: " << cpuPath << "\n";
        return false;
    }

    // Read header
    cpuFile.read(reinterpret_cast<char*>(&m_header), sizeof(PegHeader));

    if (m_header.signature != PEG_SIGNATURE) {
        std::cerr << "Invalid PEG signature: 0x" << std::hex << m_header.signature
                  << " (expected 0x" << PEG_SIGNATURE << ")\n";
        return false;
    }

    if (m_header.version != PEG_VERSION) {
        std::cerr << "Warning: PEG version " << m_header.version
                  << " (expected " << PEG_VERSION << ")\n";
    }

    // Read texture entries
    std::vector<PegTextureEntry> entries(m_header.num_textures);
    cpuFile.read(reinterpret_cast<char*>(entries.data()),
                 m_header.num_textures * sizeof(PegTextureEntry));

    // Read name table (follows entries)
    size_t nameTableOffset = sizeof(PegHeader) +
                             m_header.num_textures * sizeof(PegTextureEntry);
    size_t nameTableSize = m_header.header_size - nameTableOffset;

    std::vector<char> names(nameTableSize);
    cpuFile.seekg(nameTableOffset);
    cpuFile.read(names.data(), nameTableSize);

    // Build texture list
    m_textures.clear();
    m_textures.reserve(m_header.num_textures);

    for (const auto& entry : entries) {
        PegTexture tex;

        // Extract name
        if (entry.name_offset < nameTableSize) {
            tex.name = &names[entry.name_offset];
        }

        tex.width = entry.width;
        tex.height = entry.height;
        tex.source_width = entry.source_width;
        tex.source_height = entry.source_height;
        tex.format = static_cast<PegFormat>(entry.format);
        tex.mip_levels = entry.mip_levels;
        tex.frames = entry.frames;
        tex.frame_delay = entry.frame_delay;
        tex.data_offset = entry.data_offset;
        tex.data_size = entry.data_size;

        m_textures.push_back(std::move(tex));
    }

    cpuFile.close();

    // Open GPU data file
    m_gpuFile.open(gpuPath, std::ios::binary);
    if (!m_gpuFile.is_open()) {
        std::cerr << "Failed to open GPU file: " << gpuPath << "\n";
        m_textures.clear();
        return false;
    }

    m_cpuPath = cpuPath;
    m_gpuPath = gpuPath;

    std::cout << "Opened PEG: " << cpuPath.filename() << " ("
              << m_textures.size() << " textures)\n";

    return true;
}

void PegArchive::close() {
    if (m_gpuFile.is_open()) {
        m_gpuFile.close();
    }
    m_textures.clear();
    m_header = {};
    m_cpuPath.clear();
    m_gpuPath.clear();
}

const PegTexture* PegArchive::findTexture(const std::string& name) const {
    for (const auto& tex : m_textures) {
        if (tex.name == name) {
            return &tex;
        }
    }
    return nullptr;
}

std::vector<uint8_t> PegArchive::extractRaw(size_t index) {
    if (index >= m_textures.size() || !m_gpuFile.is_open()) {
        return {};
    }

    const auto& tex = m_textures[index];
    std::vector<uint8_t> data(tex.data_size);

    m_gpuFile.seekg(tex.data_offset);
    m_gpuFile.read(reinterpret_cast<char*>(data.data()), tex.data_size);

    return data;
}

std::vector<uint8_t> PegArchive::extractRaw(const std::string& name) {
    for (size_t i = 0; i < m_textures.size(); ++i) {
        if (m_textures[i].name == name) {
            return extractRaw(i);
        }
    }
    return {};
}

std::vector<uint8_t> PegArchive::extractRGBA(size_t index) {
    if (index >= m_textures.size()) {
        return {};
    }

    auto raw = extractRaw(index);
    if (raw.empty()) {
        return {};
    }

    const auto& tex = m_textures[index];
    std::vector<uint8_t> rgba;

    switch (tex.format) {
        case PegFormat::DXT1:
            rgba = decodeDXT1(raw.data(), tex.width, tex.height);
            break;
        case PegFormat::DXT3:
            rgba = decodeDXT3(raw.data(), tex.width, tex.height);
            break;
        case PegFormat::DXT5:
            rgba = decodeDXT5(raw.data(), tex.width, tex.height);
            break;
        case PegFormat::A8R8G8B8:
            // Convert ARGB to RGBA
            rgba.resize(tex.width * tex.height * 4);
            for (size_t i = 0; i < tex.width * tex.height; ++i) {
                rgba[i * 4 + 0] = raw[i * 4 + 1]; // R
                rgba[i * 4 + 1] = raw[i * 4 + 2]; // G
                rgba[i * 4 + 2] = raw[i * 4 + 3]; // B
                rgba[i * 4 + 3] = raw[i * 4 + 0]; // A
            }
            break;
        case PegFormat::A8:
            // Single channel alpha to RGBA (white with alpha)
            rgba.resize(tex.width * tex.height * 4);
            for (size_t i = 0; i < tex.width * tex.height; ++i) {
                rgba[i * 4 + 0] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = raw[i];
            }
            break;
        default:
            std::cerr << "Unsupported format for RGBA conversion: "
                      << tex.formatName() << "\n";
            return raw; // Return raw data as fallback
    }

    return rgba;
}

std::vector<uint8_t> PegArchive::extractRGBA(const std::string& name) {
    for (size_t i = 0; i < m_textures.size(); ++i) {
        if (m_textures[i].name == name) {
            return extractRGBA(i);
        }
    }
    return {};
}

std::filesystem::path PegArchive::findPairedFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string pairedExt;
    if (ext == ".cpeg_pc") {
        pairedExt = ".gpeg_pc";
    } else if (ext == ".gpeg_pc") {
        pairedExt = ".cpeg_pc";
    } else if (ext == ".cvbm_pc") {
        pairedExt = ".gvbm_pc";
    } else if (ext == ".gvbm_pc") {
        pairedExt = ".cvbm_pc";
    } else {
        return {};
    }

    auto paired = path;
    paired.replace_extension(pairedExt);

    if (std::filesystem::exists(paired)) {
        return paired;
    }

    return {};
}

// DXT Decoding

std::vector<uint8_t> PegArchive::decodeDXT1(const uint8_t* data,
                                            uint32_t width, uint32_t height) {
    std::vector<uint8_t> output(width * height * 4);
    uint32_t blockCountX = (width + 3) / 4;
    uint32_t blockCountY = (height + 3) / 4;

    for (uint32_t by = 0; by < blockCountY; ++by) {
        for (uint32_t bx = 0; bx < blockCountX; ++bx) {
            const uint8_t* block = data + (by * blockCountX + bx) * 8;

            uint8_t blockOutput[4 * 4 * 4]; // 4x4 pixels, RGBA
            decodeDXT1Block(block, blockOutput, 4 * 4);

            // Copy to output
            for (uint32_t py = 0; py < 4; ++py) {
                uint32_t y = by * 4 + py;
                if (y >= height) continue;

                for (uint32_t px = 0; px < 4; ++px) {
                    uint32_t x = bx * 4 + px;
                    if (x >= width) continue;

                    size_t srcIdx = (py * 4 + px) * 4;
                    size_t dstIdx = (y * width + x) * 4;

                    output[dstIdx + 0] = blockOutput[srcIdx + 0];
                    output[dstIdx + 1] = blockOutput[srcIdx + 1];
                    output[dstIdx + 2] = blockOutput[srcIdx + 2];
                    output[dstIdx + 3] = blockOutput[srcIdx + 3];
                }
            }
        }
    }

    return output;
}

std::vector<uint8_t> PegArchive::decodeDXT3(const uint8_t* data,
                                            uint32_t width, uint32_t height) {
    std::vector<uint8_t> output(width * height * 4);
    uint32_t blockCountX = (width + 3) / 4;
    uint32_t blockCountY = (height + 3) / 4;

    for (uint32_t by = 0; by < blockCountY; ++by) {
        for (uint32_t bx = 0; bx < blockCountX; ++bx) {
            const uint8_t* block = data + (by * blockCountX + bx) * 16;

            uint8_t blockOutput[4 * 4 * 4];
            decodeDXT1Block(block + 8, blockOutput, 4 * 4);
            decodeDXT3AlphaBlock(block, blockOutput, 4 * 4);

            for (uint32_t py = 0; py < 4; ++py) {
                uint32_t y = by * 4 + py;
                if (y >= height) continue;

                for (uint32_t px = 0; px < 4; ++px) {
                    uint32_t x = bx * 4 + px;
                    if (x >= width) continue;

                    size_t srcIdx = (py * 4 + px) * 4;
                    size_t dstIdx = (y * width + x) * 4;

                    output[dstIdx + 0] = blockOutput[srcIdx + 0];
                    output[dstIdx + 1] = blockOutput[srcIdx + 1];
                    output[dstIdx + 2] = blockOutput[srcIdx + 2];
                    output[dstIdx + 3] = blockOutput[srcIdx + 3];
                }
            }
        }
    }

    return output;
}

std::vector<uint8_t> PegArchive::decodeDXT5(const uint8_t* data,
                                            uint32_t width, uint32_t height) {
    std::vector<uint8_t> output(width * height * 4);
    uint32_t blockCountX = (width + 3) / 4;
    uint32_t blockCountY = (height + 3) / 4;

    for (uint32_t by = 0; by < blockCountY; ++by) {
        for (uint32_t bx = 0; bx < blockCountX; ++bx) {
            const uint8_t* block = data + (by * blockCountX + bx) * 16;

            uint8_t blockOutput[4 * 4 * 4];
            decodeDXT1Block(block + 8, blockOutput, 4 * 4);
            decodeDXT5AlphaBlock(block, blockOutput, 4 * 4);

            for (uint32_t py = 0; py < 4; ++py) {
                uint32_t y = by * 4 + py;
                if (y >= height) continue;

                for (uint32_t px = 0; px < 4; ++px) {
                    uint32_t x = bx * 4 + px;
                    if (x >= width) continue;

                    size_t srcIdx = (py * 4 + px) * 4;
                    size_t dstIdx = (y * width + x) * 4;

                    output[dstIdx + 0] = blockOutput[srcIdx + 0];
                    output[dstIdx + 1] = blockOutput[srcIdx + 1];
                    output[dstIdx + 2] = blockOutput[srcIdx + 2];
                    output[dstIdx + 3] = blockOutput[srcIdx + 3];
                }
            }
        }
    }

    return output;
}

void PegArchive::decodeDXT1Block(const uint8_t* block, uint8_t* output, uint32_t stride) {
    uint16_t c0 = block[0] | (block[1] << 8);
    uint16_t c1 = block[2] | (block[3] << 8);

    // Decode RGB565 colors
    uint8_t colors[4][4];

    colors[0][0] = ((c0 >> 11) & 0x1F) * 255 / 31;
    colors[0][1] = ((c0 >> 5) & 0x3F) * 255 / 63;
    colors[0][2] = (c0 & 0x1F) * 255 / 31;
    colors[0][3] = 255;

    colors[1][0] = ((c1 >> 11) & 0x1F) * 255 / 31;
    colors[1][1] = ((c1 >> 5) & 0x3F) * 255 / 63;
    colors[1][2] = (c1 & 0x1F) * 255 / 31;
    colors[1][3] = 255;

    if (c0 > c1) {
        // 4-color block
        colors[2][0] = (2 * colors[0][0] + colors[1][0]) / 3;
        colors[2][1] = (2 * colors[0][1] + colors[1][1]) / 3;
        colors[2][2] = (2 * colors[0][2] + colors[1][2]) / 3;
        colors[2][3] = 255;

        colors[3][0] = (colors[0][0] + 2 * colors[1][0]) / 3;
        colors[3][1] = (colors[0][1] + 2 * colors[1][1]) / 3;
        colors[3][2] = (colors[0][2] + 2 * colors[1][2]) / 3;
        colors[3][3] = 255;
    } else {
        // 3-color block with transparency
        colors[2][0] = (colors[0][0] + colors[1][0]) / 2;
        colors[2][1] = (colors[0][1] + colors[1][1]) / 2;
        colors[2][2] = (colors[0][2] + colors[1][2]) / 2;
        colors[2][3] = 255;

        colors[3][0] = 0;
        colors[3][1] = 0;
        colors[3][2] = 0;
        colors[3][3] = 0;
    }

    // Decode indices
    uint32_t indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

    for (int i = 0; i < 16; ++i) {
        uint8_t idx = (indices >> (i * 2)) & 0x3;
        output[i * 4 + 0] = colors[idx][0];
        output[i * 4 + 1] = colors[idx][1];
        output[i * 4 + 2] = colors[idx][2];
        output[i * 4 + 3] = colors[idx][3];
    }
}

void PegArchive::decodeDXT3AlphaBlock(const uint8_t* block, uint8_t* output, uint32_t stride) {
    // DXT3: Explicit 4-bit alpha for each pixel
    for (int i = 0; i < 16; ++i) {
        uint8_t alpha;
        if (i % 2 == 0) {
            alpha = (block[i / 2] & 0x0F) * 17; // Scale 0-15 to 0-255
        } else {
            alpha = ((block[i / 2] >> 4) & 0x0F) * 17;
        }
        output[i * 4 + 3] = alpha;
    }
}

void PegArchive::decodeDXT5AlphaBlock(const uint8_t* block, uint8_t* output, uint32_t stride) {
    // DXT5: Interpolated alpha
    uint8_t a0 = block[0];
    uint8_t a1 = block[1];

    uint8_t alphas[8];
    alphas[0] = a0;
    alphas[1] = a1;

    if (a0 > a1) {
        alphas[2] = (6 * a0 + 1 * a1) / 7;
        alphas[3] = (5 * a0 + 2 * a1) / 7;
        alphas[4] = (4 * a0 + 3 * a1) / 7;
        alphas[5] = (3 * a0 + 4 * a1) / 7;
        alphas[6] = (2 * a0 + 5 * a1) / 7;
        alphas[7] = (1 * a0 + 6 * a1) / 7;
    } else {
        alphas[2] = (4 * a0 + 1 * a1) / 5;
        alphas[3] = (3 * a0 + 2 * a1) / 5;
        alphas[4] = (2 * a0 + 3 * a1) / 5;
        alphas[5] = (1 * a0 + 4 * a1) / 5;
        alphas[6] = 0;
        alphas[7] = 255;
    }

    // Read 48-bit index table
    uint64_t indices = 0;
    for (int i = 0; i < 6; ++i) {
        indices |= static_cast<uint64_t>(block[2 + i]) << (i * 8);
    }

    for (int i = 0; i < 16; ++i) {
        uint8_t idx = (indices >> (i * 3)) & 0x7;
        output[i * 4 + 3] = alphas[idx];
    }
}

} // namespace opensaints
