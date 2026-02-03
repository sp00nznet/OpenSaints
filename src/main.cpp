// OpenSaints - Saints Row 2 Reimplementation
// A clean-room reimplementation requiring original game assets

#include <iostream>
#include <iomanip>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>

#include "engine/vfs.h"
#include "engine/asset_manager.h"
#include "formats/vpp.h"
#include "formats/xtbl.h"
#include "formats/preload_table.h"

namespace fs = std::filesystem;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] <game_path>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --list-assets     List all assets in VPP archives\n";
    std::cout << "  --extract <path>  Extract all assets to path\n";
    std::cout << "  --info            Show game file information\n";
    std::cout << "  --help            Show this help message\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << programName << " \"C:/Games/Saints Row 2\"\n";
}

bool initializeVFS(opensaints::VirtualFileSystem& vfs, const fs::path& gamePath) {
    std::cout << "Initializing virtual filesystem...\n";

    // Mount all VPP archives from the game directory
    if (!vfs.mountDirectory(gamePath, "/", 0)) {
        std::cerr << "Failed to mount any archives from: " << gamePath << "\n";
        return false;
    }

    std::cout << "VFS initialized with " << vfs.getTotalFileCount() << " files\n";
    return true;
}

void listAssetTypes(const opensaints::VirtualFileSystem& vfs) {
    std::cout << "\nAsset type summary:\n";

    // Count by extension
    struct TypeCount {
        std::string extension;
        size_t count = 0;
        size_t totalSize = 0;
    };

    std::unordered_map<std::string, TypeCount> types;

    vfs.forEachFile([&types](const opensaints::VfsFileInfo& info) {
        size_t dotPos = info.path.rfind('.');
        std::string ext = (dotPos != std::string::npos) ? info.path.substr(dotPos) : "(no ext)";

        auto& tc = types[ext];
        tc.extension = ext;
        tc.count++;
        tc.totalSize += info.size;
    });

    // Sort by count
    std::vector<TypeCount> sorted;
    for (const auto& [ext, tc] : types) {
        sorted.push_back(tc);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const TypeCount& a, const TypeCount& b) { return a.count > b.count; });

    std::cout << std::string(50, '-') << "\n";
    std::cout << "Extension         Count        Total Size\n";
    std::cout << std::string(50, '-') << "\n";

    for (const auto& tc : sorted) {
        std::cout << std::left << std::setw(18) << tc.extension
                  << std::right << std::setw(8) << tc.count
                  << std::setw(16) << (tc.totalSize / 1024) << " KB\n";
    }

    std::cout << std::string(50, '-') << "\n";
}

void showGameInfo(const opensaints::VirtualFileSystem& vfs,
                  opensaints::AssetManager& assets) {
    std::cout << "\nGame Information:\n";
    std::cout << std::string(50, '-') << "\n";

    // Try to load and display some XTBL data
    auto achievementsHandle = assets.loadXTable("achievements");
    if (achievementsHandle.isLoaded()) {
        const auto* doc = achievementsHandle->document().get();
        if (doc) {
            std::cout << "Achievements: " << doc->entries().size() << " entries\n";
        }
    }

    // List available XTBL files
    auto xtblFiles = vfs.listByExtension("xtbl");
    std::cout << "Configuration tables (XTBL): " << xtblFiles.size() << " files\n";
    if (xtblFiles.size() > 0 && xtblFiles.size() <= 20) {
        for (const auto& f : xtblFiles) {
            std::cout << "  " << f << "\n";
        }
    }

    // List mesh files
    auto cmeshFiles = vfs.listByExtension("cmesh_pc");
    auto smeshFiles = vfs.listByExtension("smesh_pc");
    std::cout << "Character meshes: " << cmeshFiles.size() << " files\n";
    std::cout << "Static meshes: " << smeshFiles.size() << " files\n";

    // List texture packages
    auto pegFiles = vfs.listByExtension("cpeg_pc");
    std::cout << "Texture packages: " << pegFiles.size() << " files\n";

    // List chunk files
    auto chunkFiles = vfs.listByExtension("chunk_pc");
    std::cout << "World chunks: " << chunkFiles.size() << " files\n";

    std::cout << std::string(50, '-') << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "OpenSaints v0.1.0\n";
    std::cout << "Saints Row 2 Reimplementation\n";
    std::cout << "Requires original game assets to run.\n\n";

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Parse command line
    std::string command;
    fs::path gamePath;
    fs::path extractPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--list-assets") {
            command = "list";
        } else if (arg == "--info") {
            command = "info";
        } else if (arg == "--extract" && i + 1 < argc) {
            command = "extract";
            extractPath = argv[++i];
        } else if (arg[0] != '-') {
            gamePath = arg;
        }
    }

    if (gamePath.empty()) {
        std::cerr << "Error: No game path specified.\n";
        printUsage(argv[0]);
        return 1;
    }

    if (!fs::exists(gamePath)) {
        std::cerr << "Error: Game path does not exist: " << gamePath << "\n";
        return 1;
    }

    // Initialize VFS
    auto vfs = std::make_shared<opensaints::VirtualFileSystem>();
    if (!initializeVFS(*vfs, gamePath)) {
        return 1;
    }

    // Initialize asset manager
    opensaints::AssetManager assets;
    if (!assets.initialize(vfs)) {
        std::cerr << "Failed to initialize asset manager\n";
        return 1;
    }

    // Execute command
    if (command == "list") {
        listAssetTypes(*vfs);
    } else if (command == "info") {
        showGameInfo(*vfs, assets);
    } else if (command == "extract") {
        std::cout << "Extraction not implemented in main executable.\n";
        std::cout << "Use the extract_all.py tool instead.\n";
    } else {
        // Default: show info and prepare for game loop
        showGameInfo(*vfs, assets);

        std::cout << "\nEngine ready. Game loop not yet implemented.\n";
        std::cout << "Next steps:\n";
        std::cout << "  1. Implement renderer (SDL2 + Vulkan)\n";
        std::cout << "  2. Load and display a test mesh\n";
        std::cout << "  3. Implement world streaming\n";
    }

    // Cleanup
    assets.shutdown();
    vfs->unmountAll();

    return 0;
}
