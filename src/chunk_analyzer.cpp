// OpenSaints Chunk Analyzer
// CLI tool for reverse-engineering .chunk_pc binary format
// Usage: chunk_analyzer <command> <file_or_directory>
// Commands: header, textures, geometry, compare, hexdump

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iomanip>

namespace fs = std::filesystem;

static constexpr uint32_t CHUNK_SIGNATURE = 0xBBCACA12;

struct ChunkAnalysis {
    std::string filename;
    size_t file_size = 0;
    size_t gpu_file_size = 0;
    bool has_gpu_file = false;

    // Header fields
    uint32_t signature = 0;
    uint32_t version = 0;
    uint32_t flags = 0;
    uint32_t val_0x0C = 0;
    uint32_t val_0x10 = 0; // render item/section count?

    // GPU buffer sizes
    uint32_t gpu_vertex_size = 0; // @0x8C
    uint32_t gpu_index_size = 0;  // @0x90

    // Counts
    uint32_t val_0x94 = 0; // section/submesh count?
    uint32_t val_0x98 = 0;
    uint32_t val_0x9C = 0; // render item count?
    uint32_t val_0xA0 = 0;

    // Bounding box
    float bounds_min[3] = {};
    float bounds_max[3] = {};

    // Textures
    uint32_t texture_count = 0;
    std::vector<std::string> texture_names;

    bool valid = false;
};

static std::vector<uint8_t> readFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

template<typename T>
static T readLE(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + sizeof(T) > data.size()) return T{};
    T val;
    std::memcpy(&val, data.data() + offset, sizeof(T));
    return val;
}

static ChunkAnalysis analyzeChunk(const fs::path& path) {
    ChunkAnalysis a;
    a.filename = path.filename().string();

    auto data = readFile(path);
    if (data.size() < 0x110) {
        std::cerr << "File too small: " << path << " (" << data.size() << " bytes)\n";
        return a;
    }

    a.file_size = data.size();

    // Check GPU file
    fs::path gpuPath = path;
    std::string stem = path.stem().string();
    fs::path gpuFile = path.parent_path() / (stem + ".g_chunk_pc");
    if (fs::exists(gpuFile)) {
        a.has_gpu_file = true;
        a.gpu_file_size = fs::file_size(gpuFile);
    }

    // Header
    a.signature = readLE<uint32_t>(data, 0x00);
    a.version = readLE<uint32_t>(data, 0x04);
    a.flags = readLE<uint32_t>(data, 0x08);
    a.val_0x0C = readLE<uint32_t>(data, 0x0C);
    a.val_0x10 = readLE<uint32_t>(data, 0x10);

    // GPU sizes
    a.gpu_vertex_size = readLE<uint32_t>(data, 0x8C);
    a.gpu_index_size = readLE<uint32_t>(data, 0x90);

    // Counts
    a.val_0x94 = readLE<uint32_t>(data, 0x94);
    a.val_0x98 = readLE<uint32_t>(data, 0x98);
    a.val_0x9C = readLE<uint32_t>(data, 0x9C);
    a.val_0xA0 = readLE<uint32_t>(data, 0xA0);

    // Bounding box at 0xD4
    for (int i = 0; i < 3; i++) {
        a.bounds_min[i] = readLE<float>(data, 0xD4 + i * 4);
        a.bounds_max[i] = readLE<float>(data, 0xE0 + i * 4);
    }

    // Texture count at 0x100
    a.texture_count = readLE<uint32_t>(data, 0x100);

    // Parse texture names: scan for null-terminated strings after 0x100
    if (a.texture_count > 0 && a.texture_count < 1000) {
        // Find the first non-zero byte after 0x104
        size_t nameStart = 0x108;
        while (nameStart < data.size() && data[nameStart] == 0) {
            nameStart++;
        }

        size_t offset = nameStart;
        for (uint32_t i = 0; i < a.texture_count && offset < data.size(); i++) {
            // Find null terminator
            size_t end = offset;
            while (end < data.size() && data[end] != 0) {
                end++;
            }
            if (end > offset) {
                a.texture_names.push_back(std::string(data.begin() + offset, data.begin() + end));
            }
            offset = end + 1;
            // Skip any padding nulls between strings (usually just 1 null)
        }
    }

    a.valid = (a.signature == CHUNK_SIGNATURE);
    return a;
}

static void cmdHeader(const fs::path& path) {
    auto a = analyzeChunk(path);
    if (!a.valid) {
        std::cerr << "Invalid chunk file (bad signature)\n";
        return;
    }

    std::cout << "=== " << a.filename << " (" << a.file_size << " bytes) ===\n";
    std::cout << "  Signature:    0x" << std::hex << std::uppercase << a.signature << std::dec << "\n";
    std::cout << "  Version:      " << a.version << "\n";
    std::cout << "  Flags:        " << a.flags << "\n";
    std::cout << "  @0x0C:        " << a.val_0x0C << "\n";
    std::cout << "  @0x10:        " << a.val_0x10 << " (sections?)\n";
    std::cout << "\n";
    std::cout << "  GPU VB size:  " << a.gpu_vertex_size << " bytes (@0x8C)\n";
    std::cout << "  GPU IB size:  " << a.gpu_index_size << " bytes (@0x90)\n";
    std::cout << "  GPU total:    " << (a.gpu_vertex_size + a.gpu_index_size) << " bytes\n";
    if (a.has_gpu_file) {
        std::cout << "  .g_chunk_pc:  " << a.gpu_file_size << " bytes";
        if (a.gpu_file_size == a.gpu_vertex_size + a.gpu_index_size)
            std::cout << " [MATCH]";
        else
            std::cout << " [MISMATCH!]";
        std::cout << "\n";
    } else {
        std::cout << "  .g_chunk_pc:  not found\n";
    }
    std::cout << "\n";
    std::cout << "  @0x94:        " << a.val_0x94 << " (submesh count?)\n";
    std::cout << "  @0x98:        " << a.val_0x98 << "\n";
    std::cout << "  @0x9C:        " << a.val_0x9C << " (render items?)\n";
    std::cout << "  @0xA0:        " << a.val_0xA0 << "\n";
    std::cout << "\n";
    std::cout << "  Bounds min:   (" << a.bounds_min[0] << ", " << a.bounds_min[1] << ", " << a.bounds_min[2] << ")\n";
    std::cout << "  Bounds max:   (" << a.bounds_max[0] << ", " << a.bounds_max[1] << ", " << a.bounds_max[2] << ")\n";
    float cx = (a.bounds_min[0] + a.bounds_max[0]) * 0.5f;
    float cy = (a.bounds_min[1] + a.bounds_max[1]) * 0.5f;
    float cz = (a.bounds_min[2] + a.bounds_max[2]) * 0.5f;
    std::cout << "  Bounds center:(" << cx << ", " << cy << ", " << cz << ")\n";
    float sx = a.bounds_max[0] - a.bounds_min[0];
    float sy = a.bounds_max[1] - a.bounds_min[1];
    float sz = a.bounds_max[2] - a.bounds_min[2];
    std::cout << "  Bounds size:  (" << sx << ", " << sy << ", " << sz << ")\n";
    std::cout << "\n";
    std::cout << "  Tex count:    " << a.texture_count << "\n";
}

static void cmdTextures(const fs::path& path) {
    auto a = analyzeChunk(path);
    if (!a.valid) {
        std::cerr << "Invalid chunk file\n";
        return;
    }

    std::cout << "=== " << a.filename << " - " << a.texture_count << " textures ===\n";
    for (size_t i = 0; i < a.texture_names.size(); i++) {
        std::cout << "  [" << i << "] " << a.texture_names[i] << "\n";
    }
    if (a.texture_names.size() < a.texture_count) {
        std::cout << "  (parsed " << a.texture_names.size() << " of " << a.texture_count << ")\n";
    }
}

static void cmdGeometry(const fs::path& path) {
    auto a = analyzeChunk(path);
    if (!a.valid) {
        std::cerr << "Invalid chunk file\n";
        return;
    }

    std::cout << "=== " << a.filename << " - Geometry Analysis ===\n";
    std::cout << "  VB size: " << a.gpu_vertex_size << " bytes\n";
    std::cout << "  IB size: " << a.gpu_index_size << " bytes\n";

    if (!a.has_gpu_file || a.gpu_file_size == 0) {
        std::cout << "  No GPU data file\n";
        return;
    }

    // Read GPU data
    fs::path gpuFile = path.parent_path() / (path.stem().string() + ".g_chunk_pc");
    auto gpuData = readFile(gpuFile);

    // Try different strides
    int strides[] = {20, 24, 28, 32, 36, 40};
    uint32_t vbSize = a.gpu_vertex_size > 0 ? a.gpu_vertex_size : static_cast<uint32_t>(gpuData.size());

    std::cout << "\n  Stride analysis (VB=" << vbSize << " bytes):\n";
    for (int stride : strides) {
        if (vbSize % stride != 0) continue;
        uint32_t vertCount = vbSize / stride;
        if (vertCount == 0 || vertCount > 10000000) continue;

        // Check if first few vertices have plausible positions
        int inBounds = 0;
        int total = std::min(vertCount, 100u);
        for (int i = 0; i < total; i++) {
            size_t off = i * stride;
            if (off + 12 > gpuData.size()) break;
            float x, y, z;
            std::memcpy(&x, gpuData.data() + off, 4);
            std::memcpy(&y, gpuData.data() + off + 4, 4);
            std::memcpy(&z, gpuData.data() + off + 8, 4);

            // Check if within extended bounds (2x padding)
            float pad = 100.0f;
            if (x >= a.bounds_min[0] - pad && x <= a.bounds_max[0] + pad &&
                y >= a.bounds_min[1] - pad && y <= a.bounds_max[1] + pad &&
                z >= a.bounds_min[2] - pad && z <= a.bounds_max[2] + pad) {
                inBounds++;
            }
        }

        float pct = (total > 0) ? (100.0f * inBounds / total) : 0;
        std::cout << "    stride " << stride << ": " << vertCount << " verts, "
                  << inBounds << "/" << total << " in bounds (" << std::fixed
                  << std::setprecision(0) << pct << "%)\n";
    }

    // Detailed analysis with best stride (20)
    int bestStride = 20;
    if (vbSize >= static_cast<uint32_t>(bestStride)) {
        uint32_t vertCount = vbSize / bestStride;
        std::cout << "\n  Stride 20 vertex dump (first 10):\n";
        for (uint32_t i = 0; i < std::min(vertCount, 10u); i++) {
            size_t off = i * bestStride;
            float x, y, z;
            std::memcpy(&x, gpuData.data() + off, 4);
            std::memcpy(&y, gpuData.data() + off + 4, 4);
            std::memcpy(&z, gpuData.data() + off + 8, 4);
            uint32_t packed_normal;
            std::memcpy(&packed_normal, gpuData.data() + off + 12, 4);

            std::cout << "    v" << i << ": pos=(" << std::fixed << std::setprecision(2)
                      << x << ", " << y << ", " << z << ") normal=0x"
                      << std::hex << packed_normal << std::dec << " uv=(";
            // UV as two uint16
            uint16_t u16, v16;
            std::memcpy(&u16, gpuData.data() + off + 16, 2);
            std::memcpy(&v16, gpuData.data() + off + 18, 2);
            std::cout << u16 << ", " << v16 << ")\n";
        }
    }

    // Index buffer analysis
    if (a.gpu_index_size > 0) {
        uint32_t indexCount = a.gpu_index_size / 2;
        std::cout << "\n  Index buffer: " << indexCount << " indices (uint16)\n";
        size_t ibOffset = a.gpu_vertex_size;
        if (ibOffset + 20 <= gpuData.size()) {
            std::cout << "    First 10: ";
            for (int i = 0; i < std::min(10u, indexCount); i++) {
                uint16_t idx;
                std::memcpy(&idx, gpuData.data() + ibOffset + i * 2, 2);
                std::cout << idx << " ";
            }
            std::cout << "\n";
        }
    }
}

static void cmdCompare(const fs::path& dir) {
    std::cout << "=== Comparing all chunks in: " << dir << " ===\n\n";

    std::vector<ChunkAnalysis> chunks;
    for (const auto& entry : fs::directory_iterator(dir)) {
        std::string ext = entry.path().extension().string();
        if (ext == ".chunk_pc") {
            auto a = analyzeChunk(entry.path());
            if (a.valid) {
                chunks.push_back(std::move(a));
            }
        }
    }

    std::sort(chunks.begin(), chunks.end(),
              [](const auto& a, const auto& b) { return a.filename < b.filename; });

    // Print comparison table
    std::cout << std::left << std::setw(30) << "Filename"
              << std::right << std::setw(8) << "Size"
              << std::setw(5) << "Ver"
              << std::setw(6) << "Flags"
              << std::setw(10) << "VB"
              << std::setw(10) << "IB"
              << std::setw(6) << "GPU?"
              << std::setw(6) << "Match"
              << std::setw(6) << "@0x94"
              << std::setw(6) << "@0x9C"
              << std::setw(6) << "Tex"
              << "\n";
    std::cout << std::string(97, '-') << "\n";

    for (const auto& a : chunks) {
        bool match = a.has_gpu_file &&
                     (a.gpu_file_size == a.gpu_vertex_size + a.gpu_index_size);
        std::cout << std::left << std::setw(30) << a.filename
                  << std::right << std::setw(8) << a.file_size
                  << std::setw(5) << a.version
                  << std::setw(6) << a.flags
                  << std::setw(10) << a.gpu_vertex_size
                  << std::setw(10) << a.gpu_index_size
                  << std::setw(6) << (a.has_gpu_file ? "Y" : "N")
                  << std::setw(6) << (a.has_gpu_file ? (match ? "OK" : "BAD") : "-")
                  << std::setw(6) << a.val_0x94
                  << std::setw(6) << a.val_0x9C
                  << std::setw(6) << a.texture_count
                  << "\n";
    }

    std::cout << "\nBounds:\n";
    for (const auto& a : chunks) {
        std::cout << "  " << std::left << std::setw(30) << a.filename
                  << "min=(" << std::fixed << std::setprecision(0)
                  << a.bounds_min[0] << ", " << a.bounds_min[1] << ", " << a.bounds_min[2]
                  << ") max=(" << a.bounds_max[0] << ", " << a.bounds_max[1]
                  << ", " << a.bounds_max[2] << ")\n";
    }
}

static void cmdHexdump(const fs::path& path) {
    auto data = readFile(path);
    if (data.empty()) {
        std::cerr << "Failed to read file\n";
        return;
    }

    std::cout << "=== " << path.filename().string() << " (" << data.size() << " bytes) ===\n";

    // Print key regions
    struct Region {
        size_t offset;
        size_t size;
        const char* name;
    };
    Region regions[] = {
        {0x00, 0x20, "Header"},
        {0x80, 0x30, "GPU info"},
        {0xD0, 0x20, "Bounds"},
        {0xF8, 0x20, "Texture header"},
    };

    for (const auto& r : regions) {
        std::cout << "\n--- " << r.name << " (0x" << std::hex << r.offset << ") ---\n";
        for (size_t i = 0; i < r.size && r.offset + i < data.size(); i += 16) {
            std::cout << "  " << std::hex << std::setfill('0') << std::setw(4)
                      << (r.offset + i) << ": ";
            // Hex
            for (size_t j = 0; j < 16 && r.offset + i + j < data.size(); j++) {
                std::cout << std::setw(2) << (int)data[r.offset + i + j] << " ";
                if (j == 7) std::cout << " ";
            }
            std::cout << " ";
            // ASCII
            for (size_t j = 0; j < 16 && r.offset + i + j < data.size(); j++) {
                uint8_t c = data[r.offset + i + j];
                std::cout << (char)((c >= 32 && c < 127) ? c : '.');
            }
            std::cout << "\n";
        }
        // Interpretations
        for (size_t j = 0; j + 3 < r.size && r.offset + j + 3 < data.size(); j += 4) {
            uint32_t u32 = readLE<uint32_t>(data, r.offset + j);
            float f32 = readLE<float>(data, r.offset + j);
            bool plausibleFloat = std::isfinite(f32) && std::abs(f32) > 0.001f && std::abs(f32) < 100000.0f;
            if (u32 != 0) {
                std::cout << "    @0x" << std::hex << (r.offset + j) << std::dec
                          << ": u32=" << u32;
                if (plausibleFloat) std::cout << " f32=" << std::fixed << std::setprecision(2) << f32;
                std::cout << " hex=0x" << std::hex << std::uppercase << u32 << std::dec << "\n";
            }
        }
    }
    std::cout << std::dec;
}

static void printUsage(const char* prog) {
    std::cout << "OpenSaints Chunk Analyzer\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << prog << " header <file.chunk_pc>       - Parse and display header\n";
    std::cout << "  " << prog << " textures <file.chunk_pc>     - List texture names\n";
    std::cout << "  " << prog << " geometry <file.chunk_pc>     - Analyze GPU geometry data\n";
    std::cout << "  " << prog << " compare <directory>           - Compare all chunks in directory\n";
    std::cout << "  " << prog << " hexdump <file.chunk_pc>      - Hex dump of key regions\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];
    fs::path path = argv[2];

    if (!fs::exists(path)) {
        std::cerr << "Path does not exist: " << path << "\n";
        return 1;
    }

    if (cmd == "header") {
        cmdHeader(path);
    } else if (cmd == "textures") {
        cmdTextures(path);
    } else if (cmd == "geometry") {
        cmdGeometry(path);
    } else if (cmd == "compare") {
        cmdCompare(path);
    } else if (cmd == "hexdump") {
        cmdHexdump(path);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
