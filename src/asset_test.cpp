// Asset Loading Test
// Tests VPP and PEG parsers with actual Saints Row 2 files

#include "formats/vpp.h"
#include "formats/peg.h"
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
    std::cout << "  extract-tex <file.cpeg_pc> <name> <out.raw>  Extract texture\n";
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
        printf("  %-30s %4ux%-4u  %-8s %d\n",
               tex.name.c_str(),
               tex.width, tex.height,
               tex.formatName(),
               tex.mip_levels);
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

    // Write simple TGA header for RGBA
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
