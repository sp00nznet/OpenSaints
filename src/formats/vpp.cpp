#include "vpp.h"
#include <iostream>
#include <cstring>

namespace opensaints {

bool VppArchive::open(const std::filesystem::path& path) {
    close();

    m_file.open(path, std::ios::binary);
    if (!m_file.is_open()) {
        std::cerr << "Failed to open: " << path << "\n";
        return false;
    }

    m_path = path;

    // Read header
    m_file.read(reinterpret_cast<char*>(&m_header), sizeof(VppHeader));

    if (m_header.magic != VPP_MAGIC) {
        std::cerr << "Invalid VPP magic: 0x" << std::hex << m_header.magic
                  << " (expected 0x" << VPP_MAGIC << ")\n";
        close();
        return false;
    }

    if (m_header.version != VPP_VERSION) {
        std::cerr << "Warning: VPP version " << m_header.version
                  << " (expected " << VPP_VERSION << ")\n";
    }

    // Seek to offset table (starts at 0x800)
    m_file.seekg(VPP_ALIGNMENT);

    // Read file entries
    std::vector<VppFileEntry> entries(m_header.num_files);
    m_file.read(reinterpret_cast<char*>(entries.data()),
                m_header.num_files * sizeof(VppFileEntry));

    // Calculate section offsets
    size_t offsets_end = VPP_ALIGNMENT + m_header.len_offsets;
    size_t filenames_start = alignTo(offsets_end);
    size_t filenames_end = filenames_start + m_header.len_filenames;
    size_t extensions_start = alignTo(filenames_end);
    size_t extensions_end = extensions_start + m_header.len_extensions;
    size_t data_start = alignTo(extensions_end);

    // Read filename section
    std::vector<char> filenames(m_header.len_filenames);
    m_file.seekg(filenames_start);
    m_file.read(filenames.data(), m_header.len_filenames);

    // Read extension section
    std::vector<char> extensions(m_header.len_extensions);
    m_file.seekg(extensions_start);
    m_file.read(extensions.data(), m_header.len_extensions);

    // Build file info list
    m_files.clear();
    m_files.reserve(m_header.num_files);

    for (const auto& entry : entries) {
        VppFileInfo info;

        // Extract null-terminated strings
        if (entry.name_offset < filenames.size()) {
            info.filename = &filenames[entry.name_offset];
        }
        if (entry.ext_offset < extensions.size()) {
            info.extension = &extensions[entry.ext_offset];
        }

        info.data_offset = data_start + entry.data_offset;
        info.data_size = entry.data_size;

        m_files.push_back(std::move(info));
    }

    std::cout << "Opened VPP: " << path.filename() << " ("
              << m_files.size() << " files)\n";

    return true;
}

void VppArchive::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    m_files.clear();
    m_header = {};
    m_path.clear();
}

std::vector<uint8_t> VppArchive::extract(size_t index) {
    if (index >= m_files.size() || !m_file.is_open()) {
        return {};
    }

    const auto& info = m_files[index];
    std::vector<uint8_t> data(info.data_size);

    m_file.seekg(info.data_offset);
    m_file.read(reinterpret_cast<char*>(data.data()), info.data_size);

    return data;
}

bool VppArchive::extractTo(size_t index, const std::filesystem::path& outputPath) {
    auto data = extract(index);
    if (data.empty() && m_files[index].data_size > 0) {
        return false;
    }

    // Create parent directories
    std::filesystem::create_directories(outputPath.parent_path());

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool VppArchive::extractAll(const std::filesystem::path& outputDir) {
    std::filesystem::create_directories(outputDir);

    for (size_t i = 0; i < m_files.size(); ++i) {
        const auto& info = m_files[i];
        std::string fullname = info.filename;
        if (!info.extension.empty()) {
            fullname += "." + info.extension;
        }

        auto outPath = outputDir / fullname;

        if (!extractTo(i, outPath)) {
            std::cerr << "Failed to extract: " << fullname << "\n";
            return false;
        }
    }

    std::cout << "Extracted " << m_files.size() << " files to " << outputDir << "\n";
    return true;
}

} // namespace opensaints
