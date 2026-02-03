#include "xtbl.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace opensaints {

// XtblNode implementation

std::optional<bool> XtblNode::asBool() const {
    if (auto* b = std::get_if<bool>(&m_value)) {
        return *b;
    }
    if (auto* s = std::get_if<std::string>(&m_value)) {
        std::string lower = *s;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "true" || lower == "1" || lower == "yes") {
            return true;
        }
        if (lower == "false" || lower == "0" || lower == "no") {
            return false;
        }
    }
    return std::nullopt;
}

std::optional<int64_t> XtblNode::asInt() const {
    if (auto* i = std::get_if<int64_t>(&m_value)) {
        return *i;
    }
    if (auto* s = std::get_if<std::string>(&m_value)) {
        try {
            return std::stoll(*s);
        } catch (...) {}
    }
    return std::nullopt;
}

std::optional<double> XtblNode::asFloat() const {
    if (auto* d = std::get_if<double>(&m_value)) {
        return *d;
    }
    if (auto* i = std::get_if<int64_t>(&m_value)) {
        return static_cast<double>(*i);
    }
    if (auto* s = std::get_if<std::string>(&m_value)) {
        try {
            return std::stod(*s);
        } catch (...) {}
    }
    return std::nullopt;
}

std::optional<std::string> XtblNode::asString() const {
    if (auto* s = std::get_if<std::string>(&m_value)) {
        return *s;
    }
    if (auto* i = std::get_if<int64_t>(&m_value)) {
        return std::to_string(*i);
    }
    if (auto* d = std::get_if<double>(&m_value)) {
        return std::to_string(*d);
    }
    if (auto* b = std::get_if<bool>(&m_value)) {
        return *b ? "true" : "false";
    }
    return std::nullopt;
}

void XtblNode::setAttribute(const std::string& key, const std::string& value) {
    m_attributes[key] = value;
}

std::optional<std::string> XtblNode::attribute(const std::string& key) const {
    auto it = m_attributes.find(key);
    if (it != m_attributes.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool XtblNode::hasAttribute(const std::string& key) const {
    return m_attributes.find(key) != m_attributes.end();
}

XtblNode* XtblNode::addChild(const std::string& name) {
    auto child = std::make_unique<XtblNode>(name);
    XtblNode* ptr = child.get();
    m_children.push_back(std::move(child));
    return ptr;
}

XtblNode* XtblNode::addChild(std::unique_ptr<XtblNode> child) {
    XtblNode* ptr = child.get();
    m_children.push_back(std::move(child));
    return ptr;
}

XtblNode* XtblNode::findChild(const std::string& name) {
    for (auto& child : m_children) {
        if (child->name() == name) {
            return child.get();
        }
    }
    return nullptr;
}

const XtblNode* XtblNode::findChild(const std::string& name) const {
    for (const auto& child : m_children) {
        if (child->name() == name) {
            return child.get();
        }
    }
    return nullptr;
}

std::vector<XtblNode*> XtblNode::findChildren(const std::string& name) {
    std::vector<XtblNode*> result;
    for (auto& child : m_children) {
        if (child->name() == name) {
            result.push_back(child.get());
        }
    }
    return result;
}

std::vector<const XtblNode*> XtblNode::findChildren(const std::string& name) const {
    std::vector<const XtblNode*> result;
    for (const auto& child : m_children) {
        if (child->name() == name) {
            result.push_back(child.get());
        }
    }
    return result;
}

std::optional<std::string> XtblNode::childValue(const std::string& name) const {
    if (auto* child = findChild(name)) {
        return child->asString();
    }
    return std::nullopt;
}

std::optional<int64_t> XtblNode::childValueInt(const std::string& name) const {
    if (auto* child = findChild(name)) {
        return child->asInt();
    }
    return std::nullopt;
}

std::optional<double> XtblNode::childValueFloat(const std::string& name) const {
    if (auto* child = findChild(name)) {
        return child->asFloat();
    }
    return std::nullopt;
}

std::optional<bool> XtblNode::childValueBool(const std::string& name) const {
    if (auto* child = findChild(name)) {
        return child->asBool();
    }
    return std::nullopt;
}

// XtblDocument implementation

bool XtblDocument::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_errors.push_back("Failed to open file: " + path.string());
        return false;
    }

    m_path = path;
    m_tableName = path.stem().string();

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

bool XtblDocument::loadFromMemory(const uint8_t* data, size_t size) {
    std::string content(reinterpret_cast<const char*>(data), size);
    return loadFromString(content);
}

bool XtblDocument::loadFromString(const std::string& content) {
    m_root.reset();
    m_errors.clear();

    if (!parseXml(content)) {
        return false;
    }

    // Extract table name from root element if not set
    if (m_tableName.empty() && m_root) {
        m_tableName = m_root->name();
    }

    std::cout << "Loaded XTBL: " << m_tableName;
    if (!m_path.empty()) {
        std::cout << " from " << m_path.filename();
    }
    if (m_root) {
        std::cout << " (" << m_root->children().size() << " top-level elements)";
    }
    std::cout << "\n";

    return true;
}

bool XtblDocument::parseXml(const std::string& content) {
    size_t pos = 0;

    // Skip BOM if present
    if (content.size() >= 3 &&
        static_cast<uint8_t>(content[0]) == 0xEF &&
        static_cast<uint8_t>(content[1]) == 0xBB &&
        static_cast<uint8_t>(content[2]) == 0xBF) {
        pos = 3;
    }

    skipWhitespace(content, pos);

    // Skip XML declaration if present
    if (pos + 5 < content.size() && content.substr(pos, 5) == "<?xml") {
        size_t end = content.find("?>", pos);
        if (end != std::string::npos) {
            pos = end + 2;
        }
    }

    skipWhitespace(content, pos);

    // Skip DOCTYPE if present
    if (pos + 9 < content.size() && content.substr(pos, 9) == "<!DOCTYPE") {
        size_t end = content.find(">", pos);
        if (end != std::string::npos) {
            pos = end + 1;
        }
    }

    skipWhitespace(content, pos);
    skipComment(content, pos);
    skipWhitespace(content, pos);

    // Parse root element
    m_root.reset(parseElement(content, pos));

    return m_root != nullptr;
}

XtblNode* XtblDocument::parseElement(const std::string& content, size_t& pos) {
    skipWhitespace(content, pos);
    skipComment(content, pos);
    skipWhitespace(content, pos);

    if (pos >= content.size() || content[pos] != '<') {
        return nullptr;
    }

    // Skip opening '<'
    ++pos;

    // Check for special tags
    if (pos < content.size() && (content[pos] == '!' || content[pos] == '?')) {
        // Skip processing instruction or comment
        size_t end = content.find('>', pos);
        if (end != std::string::npos) {
            pos = end + 1;
        }
        return parseElement(content, pos); // Try next element
    }

    // Parse tag name
    std::string tagName = parseTagName(content, pos);
    if (tagName.empty()) {
        m_errors.push_back("Empty tag name at position " + std::to_string(pos));
        return nullptr;
    }

    auto node = std::make_unique<XtblNode>(tagName);

    // Parse attributes
    auto attrs = parseAttributes(content, pos);
    for (const auto& [key, value] : attrs) {
        node->setAttribute(key, value);
    }

    skipWhitespace(content, pos);

    // Check for self-closing tag
    if (pos < content.size() && content[pos] == '/') {
        ++pos;
        if (pos < content.size() && content[pos] == '>') {
            ++pos;
            return node.release();
        }
    }

    // Expect closing '>'
    if (pos >= content.size() || content[pos] != '>') {
        m_errors.push_back("Expected '>' at position " + std::to_string(pos));
        return nullptr;
    }
    ++pos;

    // Parse content and children
    std::string endTag = "</" + tagName + ">";
    std::string textContent;

    while (pos < content.size()) {
        skipWhitespace(content, pos);

        // Check for end tag
        if (pos + endTag.size() <= content.size() &&
            content.substr(pos, endTag.size()) == endTag) {
            pos += endTag.size();
            break;
        }

        // Check for child element
        if (pos < content.size() && content[pos] == '<') {
            // Check for comment
            if (pos + 4 < content.size() && content.substr(pos, 4) == "<!--") {
                skipComment(content, pos);
                continue;
            }

            // Check for end tag (without matching name - error)
            if (pos + 2 < content.size() && content.substr(pos, 2) == "</") {
                m_errors.push_back("Unexpected end tag at position " + std::to_string(pos));
                break;
            }

            // Parse child element
            if (XtblNode* child = parseElement(content, pos)) {
                node->addChild(std::unique_ptr<XtblNode>(child));
            }
        } else {
            // Parse text content
            size_t textEnd = content.find('<', pos);
            if (textEnd == std::string::npos) {
                textEnd = content.size();
            }
            textContent += content.substr(pos, textEnd - pos);
            pos = textEnd;
        }
    }

    // Set text content as value if present
    // Trim whitespace
    size_t start = textContent.find_first_not_of(" \t\r\n");
    if (start != std::string::npos) {
        size_t end = textContent.find_last_not_of(" \t\r\n");
        textContent = textContent.substr(start, end - start + 1);
        textContent = decodeXmlEntities(textContent);
        node->setValue(textContent);
    }

    return node.release();
}

std::string XtblDocument::parseTagName(const std::string& content, size_t& pos) {
    size_t start = pos;
    while (pos < content.size()) {
        char c = content[pos];
        if (std::isalnum(c) || c == '_' || c == '-' || c == ':' || c == '.') {
            ++pos;
        } else {
            break;
        }
    }
    return content.substr(start, pos - start);
}

std::unordered_map<std::string, std::string> XtblDocument::parseAttributes(
    const std::string& content, size_t& pos) {
    std::unordered_map<std::string, std::string> attrs;

    while (pos < content.size()) {
        skipWhitespace(content, pos);

        // Check for end of tag
        if (pos >= content.size() || content[pos] == '>' || content[pos] == '/') {
            break;
        }

        // Parse attribute name
        std::string name = parseTagName(content, pos);
        if (name.empty()) {
            break;
        }

        skipWhitespace(content, pos);

        // Expect '='
        if (pos >= content.size() || content[pos] != '=') {
            break;
        }
        ++pos;

        skipWhitespace(content, pos);

        // Parse attribute value (quoted)
        if (pos >= content.size()) {
            break;
        }

        char quote = content[pos];
        if (quote != '"' && quote != '\'') {
            break;
        }
        ++pos;

        size_t valueStart = pos;
        while (pos < content.size() && content[pos] != quote) {
            ++pos;
        }

        std::string value = content.substr(valueStart, pos - valueStart);
        value = decodeXmlEntities(value);
        attrs[name] = value;

        if (pos < content.size()) {
            ++pos; // Skip closing quote
        }
    }

    return attrs;
}

std::string XtblDocument::parseTextContent(const std::string& content, size_t& pos,
                                           const std::string& endTag) {
    std::string text;
    while (pos < content.size()) {
        if (content.substr(pos, endTag.size()) == endTag) {
            break;
        }
        text += content[pos++];
    }
    return text;
}

void XtblDocument::skipWhitespace(const std::string& content, size_t& pos) {
    while (pos < content.size() && std::isspace(content[pos])) {
        ++pos;
    }
}

void XtblDocument::skipComment(const std::string& content, size_t& pos) {
    while (pos + 4 < content.size() && content.substr(pos, 4) == "<!--") {
        size_t end = content.find("-->", pos + 4);
        if (end != std::string::npos) {
            pos = end + 3;
            skipWhitespace(content, pos);
        } else {
            break;
        }
    }
}

std::string XtblDocument::decodeXmlEntities(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '&') {
            if (str.substr(i, 4) == "&lt;") {
                result += '<';
                i += 3;
            } else if (str.substr(i, 4) == "&gt;") {
                result += '>';
                i += 3;
            } else if (str.substr(i, 5) == "&amp;") {
                result += '&';
                i += 4;
            } else if (str.substr(i, 6) == "&apos;") {
                result += '\'';
                i += 5;
            } else if (str.substr(i, 6) == "&quot;") {
                result += '"';
                i += 5;
            } else if (str.substr(i, 2) == "&#") {
                // Numeric entity
                size_t end = str.find(';', i);
                if (end != std::string::npos) {
                    std::string num = str.substr(i + 2, end - i - 2);
                    try {
                        int code;
                        if (!num.empty() && (num[0] == 'x' || num[0] == 'X')) {
                            code = std::stoi(num.substr(1), nullptr, 16);
                        } else {
                            code = std::stoi(num);
                        }
                        if (code < 128) {
                            result += static_cast<char>(code);
                        }
                        i = end;
                    } catch (...) {
                        result += str[i];
                    }
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

std::vector<XtblNode*> XtblDocument::entries() {
    std::vector<XtblNode*> result;
    if (!m_root) {
        return result;
    }

    // Try common entry container names
    static const char* containerNames[] = {
        "Table", "Entries", "Items", "Data", nullptr
    };

    XtblNode* container = nullptr;
    for (const char** name = containerNames; *name; ++name) {
        container = m_root->findChild(*name);
        if (container) {
            break;
        }
    }

    // If no container found, use root's children directly
    if (!container) {
        container = m_root.get();
    }

    // Get all children as entries
    for (auto& child : container->children()) {
        result.push_back(child.get());
    }

    return result;
}

std::vector<const XtblNode*> XtblDocument::entries() const {
    std::vector<const XtblNode*> result;
    if (!m_root) {
        return result;
    }

    static const char* containerNames[] = {
        "Table", "Entries", "Items", "Data", nullptr
    };

    const XtblNode* container = nullptr;
    for (const char** name = containerNames; *name; ++name) {
        container = m_root->findChild(*name);
        if (container) {
            break;
        }
    }

    if (!container) {
        container = m_root.get();
    }

    for (const auto& child : container->children()) {
        result.push_back(child.get());
    }

    return result;
}

XtblNode* XtblDocument::findEntryByName(const std::string& name) {
    for (auto* entry : entries()) {
        if (auto nameVal = entry->childValue("Name")) {
            if (*nameVal == name) {
                return entry;
            }
        }
        // Also check name attribute
        if (auto nameAttr = entry->attribute("name")) {
            if (*nameAttr == name) {
                return entry;
            }
        }
    }
    return nullptr;
}

const XtblNode* XtblDocument::findEntryByName(const std::string& name) const {
    for (const auto* entry : entries()) {
        if (auto nameVal = entry->childValue("Name")) {
            if (*nameVal == name) {
                return entry;
            }
        }
        if (auto nameAttr = entry->attribute("name")) {
            if (*nameAttr == name) {
                return entry;
            }
        }
    }
    return nullptr;
}

// Common XTBL types

namespace xtbl {

Achievement Achievement::fromNode(const XtblNode* node) {
    Achievement a;
    if (!node) return a;

    if (auto v = node->childValue("Name")) a.name = *v;
    if (auto v = node->childValue("DisplayName")) a.displayName = *v;
    if (auto v = node->childValue("Description")) a.description = *v;
    if (auto v = node->childValueInt("Points")) a.points = static_cast<int32_t>(*v);
    if (auto v = node->childValue("ImageFile")) a.imageFile = *v;

    return a;
}

ActionNode ActionNode::fromNode(const XtblNode* node) {
    ActionNode a;
    if (!node) return a;

    if (auto v = node->childValue("Name")) a.name = *v;
    if (auto v = node->childValue("Animation")) a.animation = *v;
    if (auto v = node->childValue("Flags")) a.flags = *v;
    if (auto v = node->childValueFloat("Duration")) a.duration = static_cast<float>(*v);

    return a;
}

Vehicle Vehicle::fromNode(const XtblNode* node) {
    Vehicle v;
    if (!node) return v;

    if (auto val = node->childValue("Name")) v.name = *val;
    if (auto val = node->childValue("DisplayName")) v.displayName = *val;
    if (auto val = node->childValue("MeshFile")) v.meshFile = *val;
    if (auto val = node->childValueFloat("MaxSpeed")) v.maxSpeed = static_cast<float>(*val);
    if (auto val = node->childValueFloat("Mass")) v.mass = static_cast<float>(*val);

    return v;
}

} // namespace xtbl

} // namespace opensaints
