// VPP extraction tool for Saints Row 2
// Usage: vpp_extract <input.vpp_pc> [output_dir]

#include <iostream>
#include <string>
#include "formats/vpp.h"

void printUsage(const char* program) {
    std::cout << "OpenSaints VPP Extractor\n\n"
              << "Usage: " << program << " <input.vpp_pc> [output_dir]\n\n"
              << "Options:\n"
              << "  input.vpp_pc  - VPP archive to extract\n"
              << "  output_dir    - Output directory (default: <input>_extracted)\n"
              << "\nExamples:\n"
              << "  " << program << " common.vpp_pc\n"
              << "  " << program << " common.vpp_pc ./extracted/common\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::filesystem::path inputPath(argv[1]);
    std::filesystem::path outputDir;

    if (argc >= 3) {
        outputDir = argv[2];
    } else {
        outputDir = inputPath.stem().string() + "_extracted";
    }

    opensaints::VppArchive archive;
    if (!archive.open(inputPath)) {
        std::cerr << "Failed to open archive: " << inputPath << "\n";
        return 1;
    }

    // List files
    std::cout << "\nContents of " << inputPath.filename() << ":\n";
    std::cout << std::string(60, '-') << "\n";

    const auto& files = archive.files();
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& f = files[i];
        std::string fullname = f.filename;
        if (!f.extension.empty()) {
            fullname += "." + f.extension;
        }
        std::cout << "  [" << i << "] " << fullname
                  << " (" << f.data_size << " bytes)\n";
    }

    std::cout << std::string(60, '-') << "\n";
    std::cout << "Total: " << files.size() << " files\n\n";

    // Extract
    std::cout << "Extracting to: " << outputDir << "\n";
    if (archive.extractAll(outputDir)) {
        std::cout << "Done!\n";
        return 0;
    } else {
        std::cerr << "Extraction failed!\n";
        return 1;
    }
}
