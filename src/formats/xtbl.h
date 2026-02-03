#pragma once
// XTBL parser for Saints Row 2
// Parses XML table files (.xtbl) used for game configuration
// Examples: achievements, action nodes, NPCs, vehicles, weapons, etc.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <filesystem>
#include <memory>

namespace opensaints {

// Forward declarations
class XtblNode;

// Value types that can appear in XTBL files
using XtblValue = std::variant<
    std::monostate,                 // null/empty
    bool,                           // boolean
    int64_t,                        // integer
    double,                         // float
    std::string,                    // string
    std::vector<std::string>        // string array/flags
>;

// A node in the XTBL document tree
class XtblNode {
public:
    XtblNode() = default;
    explicit XtblNode(const std::string& name) : m_name(name) {}

    // Node name (tag name)
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Node value (text content)
    const XtblValue& value() const { return m_value; }
    void setValue(const XtblValue& value) { m_value = value; }

    // Get value as specific type (returns nullopt if wrong type)
    std::optional<bool> asBool() const;
    std::optional<int64_t> asInt() const;
    std::optional<double> asFloat() const;
    std::optional<std::string> asString() const;

    // Attributes
    const std::unordered_map<std::string, std::string>& attributes() const { return m_attributes; }
    void setAttribute(const std::string& key, const std::string& value);
    std::optional<std::string> attribute(const std::string& key) const;
    bool hasAttribute(const std::string& key) const;

    // Child nodes
    const std::vector<std::unique_ptr<XtblNode>>& children() const { return m_children; }
    XtblNode* addChild(const std::string& name);
    XtblNode* addChild(std::unique_ptr<XtblNode> child);

    // Find child by name (first match)
    XtblNode* findChild(const std::string& name);
    const XtblNode* findChild(const std::string& name) const;

    // Find all children with name
    std::vector<XtblNode*> findChildren(const std::string& name);
    std::vector<const XtblNode*> findChildren(const std::string& name) const;

    // Get child value by name (convenience)
    std::optional<std::string> childValue(const std::string& name) const;
    std::optional<int64_t> childValueInt(const std::string& name) const;
    std::optional<double> childValueFloat(const std::string& name) const;
    std::optional<bool> childValueBool(const std::string& name) const;

    // Check if this is a leaf node
    bool isLeaf() const { return m_children.empty(); }

private:
    std::string m_name;
    XtblValue m_value;
    std::unordered_map<std::string, std::string> m_attributes;
    std::vector<std::unique_ptr<XtblNode>> m_children;
};

// XTBL document
class XtblDocument {
public:
    XtblDocument() = default;
    ~XtblDocument() = default;

    // Parse from file
    bool loadFromFile(const std::filesystem::path& path);

    // Parse from memory buffer
    bool loadFromMemory(const uint8_t* data, size_t size);

    // Parse from string
    bool loadFromString(const std::string& content);

    // Get root node
    XtblNode* root() { return m_root.get(); }
    const XtblNode* root() const { return m_root.get(); }

    // Get table name (from root element or filename)
    const std::string& tableName() const { return m_tableName; }

    // Find all entries (typically <Entry> or table-specific elements)
    std::vector<XtblNode*> entries();
    std::vector<const XtblNode*> entries() const;

    // Find entry by name field
    XtblNode* findEntryByName(const std::string& name);
    const XtblNode* findEntryByName(const std::string& name) const;

    // Check if loaded
    bool isLoaded() const { return m_root != nullptr; }

    // Get source path
    const std::filesystem::path& path() const { return m_path; }

    // Get parse errors
    const std::vector<std::string>& errors() const { return m_errors; }

private:
    std::filesystem::path m_path;
    std::string m_tableName;
    std::unique_ptr<XtblNode> m_root;
    std::vector<std::string> m_errors;

    // Simple XML parser state
    bool parseXml(const std::string& content);
    XtblNode* parseElement(const std::string& content, size_t& pos);
    std::string parseTagName(const std::string& content, size_t& pos);
    std::unordered_map<std::string, std::string> parseAttributes(const std::string& content, size_t& pos);
    std::string parseTextContent(const std::string& content, size_t& pos, const std::string& endTag);
    void skipWhitespace(const std::string& content, size_t& pos);
    void skipComment(const std::string& content, size_t& pos);
    std::string decodeXmlEntities(const std::string& str);
};

// Common XTBL table types for convenience
namespace xtbl {

// Achievement entry
struct Achievement {
    std::string name;
    std::string displayName;
    std::string description;
    int32_t points = 0;
    std::string imageFile;

    static Achievement fromNode(const XtblNode* node);
};

// Action node entry
struct ActionNode {
    std::string name;
    std::string animation;
    std::string flags;
    float duration = 0.0f;

    static ActionNode fromNode(const XtblNode* node);
};

// Vehicle entry
struct Vehicle {
    std::string name;
    std::string displayName;
    std::string meshFile;
    float maxSpeed = 0.0f;
    float mass = 0.0f;

    static Vehicle fromNode(const XtblNode* node);
};

} // namespace xtbl

} // namespace opensaints
