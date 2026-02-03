#include "preload_table.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>

namespace opensaints {

bool PreloadTable::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open preload table: " << path << "\n";
        return false;
    }

    m_path = path;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool PreloadTable::loadFromMemory(const uint8_t* data, size_t size) {
    std::string content(reinterpret_cast<const char*>(data), size);
    return loadFromString(content);
}

bool PreloadTable::loadFromString(const std::string& content) {
    m_entries.clear();
    m_idIndex.clear();
    m_filenameIndex.clear();

    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    std::string currentCategory;
    int32_t priority = 0;

    while (std::getline(stream, line)) {
        ++lineNumber;

        // Handle Windows line endings
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        // Check for category header [Category]
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentCategory = trimmed.substr(1, trimmed.length() - 2);
            continue;
        }

        // Parse the entry
        PreloadEntry entry;
        entry.category = currentCategory;
        entry.priority = priority++;

        // Format variations:
        // 1. ID→"filename.ext"
        // 2. ID->"filename.ext"  (ASCII arrow)
        // 3. ID = "filename.ext"
        // 4. ID "filename.ext"
        // 5. ID filename.ext (no quotes)

        // Try to parse ID and filename
        std::regex pattern1(R"((\d+)\s*(?:→|->|=)?\s*["]([^"]+)["])");
        std::regex pattern2(R"((\d+)\s+([^\s"]+))");
        std::smatch match;

        if (std::regex_search(trimmed, match, pattern1)) {
            entry.id = std::stoul(match[1].str());
            entry.filename = match[2].str();
        } else if (std::regex_search(trimmed, match, pattern2)) {
            entry.id = std::stoul(match[1].str());
            entry.filename = match[2].str();
        } else {
            // Try simple format: just ID and filename separated by space/tab
            size_t pos = trimmed.find_first_of(" \t");
            if (pos != std::string::npos) {
                try {
                    entry.id = std::stoul(trimmed.substr(0, pos));
                    entry.filename = trim(trimmed.substr(pos + 1));
                    // Remove quotes if present
                    if (entry.filename.size() >= 2 &&
                        entry.filename.front() == '"' && entry.filename.back() == '"') {
                        entry.filename = entry.filename.substr(1, entry.filename.size() - 2);
                    }
                } catch (...) {
                    std::cerr << "Warning: Could not parse line " << lineNumber
                              << ": " << trimmed << "\n";
                    continue;
                }
            } else {
                std::cerr << "Warning: Could not parse line " << lineNumber
                          << ": " << trimmed << "\n";
                continue;
            }
        }

        m_entries.push_back(std::move(entry));
    }

    buildIndices();

    std::cout << "Loaded preload table: " << m_entries.size() << " entries";
    if (!m_path.empty()) {
        std::cout << " from " << m_path.filename();
    }
    std::cout << "\n";

    return true;
}

const PreloadEntry* PreloadTable::findById(uint32_t id) const {
    auto it = m_idIndex.find(id);
    if (it != m_idIndex.end()) {
        return &m_entries[it->second];
    }
    return nullptr;
}

const PreloadEntry* PreloadTable::findByFilename(const std::string& filename) const {
    auto it = m_filenameIndex.find(filename);
    if (it != m_filenameIndex.end()) {
        return &m_entries[it->second];
    }
    return nullptr;
}

std::vector<const PreloadEntry*> PreloadTable::getByCategory(const std::string& category) const {
    std::vector<const PreloadEntry*> result;
    for (const auto& entry : m_entries) {
        if (entry.category == category) {
            result.push_back(&entry);
        }
    }
    return result;
}

std::vector<const PreloadEntry*> PreloadTable::getSortedByPriority() const {
    std::vector<const PreloadEntry*> result;
    result.reserve(m_entries.size());
    for (const auto& entry : m_entries) {
        result.push_back(&entry);
    }
    std::sort(result.begin(), result.end(),
              [](const PreloadEntry* a, const PreloadEntry* b) {
                  return a->priority < b->priority;
              });
    return result;
}

bool PreloadTable::parseLine(const std::string& line, int lineNumber) {
    // This is now handled in loadFromString
    return true;
}

void PreloadTable::buildIndices() {
    m_idIndex.clear();
    m_filenameIndex.clear();

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& entry = m_entries[i];
        m_idIndex[entry.id] = i;
        m_filenameIndex[entry.filename] = i;
    }
}

std::string PreloadTable::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace opensaints
