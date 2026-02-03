#pragma once
// Preload table parser for Saints Row 2
// Parses preload.tbl and preload_anim.tbl files
// Format: Text-based, ID→"filename.extension" pattern

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace opensaints {

// A single entry in the preload table
struct PreloadEntry {
    uint32_t    id;         // Numeric ID for the asset
    std::string filename;   // Full filename with extension
    std::string category;   // Optional category (for grouped entries)
    int32_t     priority;   // Load priority (lower = earlier)
};

// Parsed preload table
class PreloadTable {
public:
    PreloadTable() = default;
    ~PreloadTable() = default;

    // Parse a preload table from file
    bool loadFromFile(const std::filesystem::path& path);

    // Parse a preload table from memory buffer
    bool loadFromMemory(const uint8_t* data, size_t size);

    // Parse a preload table from string
    bool loadFromString(const std::string& content);

    // Get all entries
    const std::vector<PreloadEntry>& entries() const { return m_entries; }

    // Look up entry by ID
    const PreloadEntry* findById(uint32_t id) const;

    // Look up entry by filename
    const PreloadEntry* findByFilename(const std::string& filename) const;

    // Get entries by category
    std::vector<const PreloadEntry*> getByCategory(const std::string& category) const;

    // Get sorted entries by priority
    std::vector<const PreloadEntry*> getSortedByPriority() const;

    // Get number of entries
    size_t count() const { return m_entries.size(); }

    // Check if loaded
    bool isLoaded() const { return !m_entries.empty(); }

    // Get source path
    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
    std::vector<PreloadEntry> m_entries;
    std::unordered_map<uint32_t, size_t> m_idIndex;
    std::unordered_map<std::string, size_t> m_filenameIndex;

    // Parse a single line
    bool parseLine(const std::string& line, int lineNumber);

    // Build lookup indices
    void buildIndices();

    // Trim whitespace
    static std::string trim(const std::string& str);
};

} // namespace opensaints
