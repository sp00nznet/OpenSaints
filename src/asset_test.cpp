// Asset Loading Test
// Tests VPP and PEG parsers with actual Saints Row 2 files

#include "formats/vpp.h"
#include "formats/peg.h"
#include "formats/mesh.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> <args...>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  list-vpp <file.vpp_pc>           List contents of VPP archive\n";
    std::cout << "  extract-vpp <file.vpp_pc> <dir>  Extract VPP to directory\n";
    std::cout << "  list-peg <file.cpeg_pc>          List textures in PEG archive\n";
    std::cout << "  extract-tex <file.cpeg_pc> <name> <out.bmp>  Extract texture\n";
    std::cout << "  load-mesh <file.cmesh_pc>        Load and analyze mesh\n";
    std::cout << "  export-mesh <file.cmesh_pc> <out.obj>  Export mesh to OBJ\n";
    std::cout << "  find-pegs <dir>                  Find all PEG files in directory\n";
    std::cout << "  scan-game <sr2-path>             Scan game directory for assets\n";
}

int listVpp(const fs::path& vppPath) {
    opensaints::VppArchive vpp;
    if (!vpp.open(vppPath)) {
        return 1;
    }

    std::cout << "\nContents of " << vppPath.filename() << ":\n";
    std::cout << std::string(60, '-') << "\n";

    size_t totalSize = 0;
    for (size_t i = 0; i < vpp.fileCount(); ++i) {
        const auto& file = vpp.files()[i];
        std::string fullName = file.filename;
        if (!file.extension.empty()) {
            fullName += "." + file.extension;
        }
        std::cout << "  " << fullName << " (" << file.data_size << " bytes)\n";
        totalSize += file.data_size;
    }

    std::cout << std::string(60, '-') << "\n";
    std::cout << "Total: " << vpp.fileCount() << " files, " << totalSize << " bytes\n";

    return 0;
}

int extractVpp(const fs::path& vppPath, const fs::path& outDir) {
    opensaints::VppArchive vpp;
    if (!vpp.open(vppPath)) {
        return 1;
    }

    if (!vpp.extractAll(outDir)) {
        return 1;
    }

    return 0;
}

int listPeg(const fs::path& pegPath) {
    opensaints::PegArchive peg;
    if (!peg.open(pegPath)) {
        return 1;
    }

    std::cout << "\nTextures in " << pegPath.filename() << ":\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << "  Name                          Size       Format   Mips\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& tex : peg.textures()) {
        printf("  %-30s %4ux%-4u  %-8s %d  off=%u size=%zu\n",
               tex.name.c_str(),
               tex.width, tex.height,
               tex.formatName(),
               tex.mip_levels,
               (unsigned)tex.data_offset,
               tex.data_size);
    }

    std::cout << std::string(70, '-') << "\n";
    std::cout << "Total: " << peg.textureCount() << " textures\n";

    return 0;
}

int extractTexture(const fs::path& pegPath, const std::string& texName,
                   const fs::path& outPath) {
    opensaints::PegArchive peg;
    if (!peg.open(pegPath)) {
        return 1;
    }

    auto rgba = peg.extractRGBA(texName);
    if (rgba.empty()) {
        std::cerr << "Texture not found or failed to decode: " << texName << "\n";
        return 1;
    }

    const auto* tex = peg.findTexture(texName);
    if (!tex) {
        return 1;
    }

    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Failed to create output file\n";
        return 1;
    }

    // Check output extension to determine format
    std::string outExt = outPath.extension().string();
    std::transform(outExt.begin(), outExt.end(), outExt.begin(), ::tolower);

    if (outExt == ".bmp") {
        // Write BMP file (more universally readable)
        #pragma pack(push, 1)
        struct BMPHeader {
            uint16_t type = 0x4D42;      // "BM"
            uint32_t fileSize;
            uint16_t reserved1 = 0;
            uint16_t reserved2 = 0;
            uint32_t dataOffset = 54;
            uint32_t headerSize = 40;
            int32_t width;
            int32_t height;
            uint16_t planes = 1;
            uint16_t bpp = 32;
            uint32_t compression = 0;
            uint32_t imageSize;
            int32_t xPixelsPerMeter = 2835;
            int32_t yPixelsPerMeter = 2835;
            uint32_t colorsUsed = 0;
            uint32_t importantColors = 0;
        };
        #pragma pack(pop)

        BMPHeader bmpHeader;
        bmpHeader.width = tex->width;
        bmpHeader.height = tex->height;  // Positive = bottom-up (BMP default)
        bmpHeader.imageSize = tex->width * tex->height * 4;
        bmpHeader.fileSize = 54 + bmpHeader.imageSize;

        out.write(reinterpret_cast<char*>(&bmpHeader), sizeof(bmpHeader));

        // BMP uses BGRA order and is stored bottom-up
        for (int y = tex->height - 1; y >= 0; --y) {
            for (uint32_t x = 0; x < tex->width; ++x) {
                size_t i = (y * tex->width + x) * 4;
                uint8_t bgra[4] = {rgba[i+2], rgba[i+1], rgba[i+0], rgba[i+3]};
                out.write(reinterpret_cast<char*>(bgra), 4);
            }
        }
    } else {
        // Write TGA file
        uint8_t header[18] = {0};
        header[2] = 2;  // Uncompressed true-color
        header[12] = tex->width & 0xFF;
        header[13] = (tex->width >> 8) & 0xFF;
        header[14] = tex->height & 0xFF;
        header[15] = (tex->height >> 8) & 0xFF;
        header[16] = 32; // Bits per pixel
        header[17] = 0x28; // Top-left origin, 8-bit alpha

        out.write(reinterpret_cast<char*>(header), 18);

        // TGA uses BGRA order
        for (size_t i = 0; i < rgba.size(); i += 4) {
            uint8_t bgra[4] = {rgba[i+2], rgba[i+1], rgba[i+0], rgba[i+3]};
            out.write(reinterpret_cast<char*>(bgra), 4);
        }
    }

    std::cout << "Extracted " << texName << " to " << outPath << "\n";
    std::cout << "  Size: " << tex->width << "x" << tex->height << "\n";
    std::cout << "  Format: " << tex->formatName() << "\n";

    return 0;
}

int findPegs(const fs::path& dir) {
    std::cout << "Scanning for PEG files in " << dir << "...\n\n";

    int count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".cpeg_pc" || ext == ".cvbm_pc") {
                std::cout << "  " << fs::relative(entry.path(), dir).string() << "\n";
                count++;
            }
        }
    }

    std::cout << "\nFound " << count << " PEG files\n";
    return 0;
}

int loadMesh(const fs::path& meshPath) {
    opensaints::CharacterMesh mesh;
    if (!mesh.open(meshPath)) {
        return 1;
    }

    const auto& data = mesh.data();
    std::cout << "\nMesh: " << data.name << "\n";
    std::cout << "Skinned: " << (data.is_skinned ? "yes" : "no") << "\n";
    std::cout << "Bounding box: ("
              << data.bounding_min.x << ", " << data.bounding_min.y << ", " << data.bounding_min.z << ") - ("
              << data.bounding_max.x << ", " << data.bounding_max.y << ", " << data.bounding_max.z << ")\n";
    std::cout << "Bounding radius: " << data.bounding_radius << "\n";
    std::cout << "Materials: " << data.materials.size() << "\n";
    std::cout << "Submeshes: " << data.submeshes.size() << "\n";

    for (size_t i = 0; i < data.submeshes.size(); ++i) {
        const auto& sm = data.submeshes[i];
        std::cout << "  [" << i << "] " << sm.vertices.size() << " vertices, "
                  << sm.indices.size() << " indices ("
                  << sm.indices.size() / 3 << " triangles)\n";

        // Print first few vertices for debugging
        if (!sm.vertices.empty()) {
            std::cout << "    First vertex: ("
                      << sm.vertices[0].position.x << ", "
                      << sm.vertices[0].position.y << ", "
                      << sm.vertices[0].position.z << ")\n";
        }
    }

    return 0;
}

int exportMesh(const fs::path& meshPath, const fs::path& outPath) {
    opensaints::CharacterMesh mesh;
    if (!mesh.open(meshPath)) {
        return 1;
    }

    if (!mesh.exportOBJ(outPath)) {
        return 1;
    }

    return 0;
}

int scanGame(const fs::path& gamePath) {
    std::cout << "Scanning Saints Row 2 installation at: " << gamePath << "\n\n";

    // Check for common VPP files
    const char* vppFiles[] = {
        "common.vpp_pc",
        "patch.vpp_pc",
        "soundboot.vpp_pc",
        "sounds.vpp_pc",
        "interface.vpp_pc",
        "items.vpp_pc",
        "vehicles.vpp_pc",
        "cutscenes.vpp_pc"
    };

    std::cout << "VPP Archives:\n";
    for (const auto& name : vppFiles) {
        auto path = gamePath / name;
        if (fs::exists(path)) {
            auto size = fs::file_size(path);
            printf("  [FOUND] %-20s  %.2f MB\n", name, size / (1024.0 * 1024.0));
        } else {
            printf("  [-----] %-20s\n", name);
        }
    }

    // Try to open common.vpp_pc and count assets
    auto commonPath = gamePath / "common.vpp_pc";
    if (fs::exists(commonPath)) {
        std::cout << "\nAnalyzing common.vpp_pc...\n";

        opensaints::VppArchive vpp;
        if (vpp.open(commonPath)) {
            // Count file types
            std::map<std::string, int> typeCounts;
            for (const auto& file : vpp.files()) {
                typeCounts[file.extension]++;
            }

            std::cout << "  Asset types:\n";
            for (const auto& [ext, count] : typeCounts) {
                printf("    .%-15s %5d files\n", ext.c_str(), count);
            }
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "list-vpp" && argc >= 3) {
        return listVpp(argv[2]);
    }
    else if (cmd == "extract-vpp" && argc >= 4) {
        return extractVpp(argv[2], argv[3]);
    }
    else if (cmd == "list-peg" && argc >= 3) {
        return listPeg(argv[2]);
    }
    else if (cmd == "extract-tex" && argc >= 5) {
        return extractTexture(argv[2], argv[3], argv[4]);
    }
    else if (cmd == "load-mesh" && argc >= 3) {
        return loadMesh(argv[2]);
    }
    else if (cmd == "export-mesh" && argc >= 4) {
        return exportMesh(argv[2], argv[3]);
    }
    else if (cmd == "find-pegs" && argc >= 3) {
        return findPegs(argv[2]);
    }
    else if (cmd == "scan-game" && argc >= 3) {
        return scanGame(argv[2]);
    }
    else {
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
