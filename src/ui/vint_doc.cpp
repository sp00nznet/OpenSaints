#include "vint_doc.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace opensaints {

// VintColor implementation

VintColor VintColor::fromHex(uint32_t hex) {
    return VintColor(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f,
        ((hex >> 24) & 0xFF) / 255.0f
    );
}

// VintAnimation implementation

void VintAnimation::sample(float time, VintProperties& props) const {
    if (keyframes.empty()) return;

    // Clamp time to animation duration
    if (loop && duration > 0) {
        time = std::fmod(time, duration);
    } else {
        time = std::min(time, duration);
    }

    // Find surrounding keyframes
    size_t idx0 = 0;
    size_t idx1 = 0;

    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
            idx0 = i;
            idx1 = i + 1;
            break;
        }
    }

    // Handle edge cases
    if (time <= keyframes[0].time) {
        const auto& kf = keyframes[0];
        props.position = kf.position;
        props.scale = kf.scale;
        props.rotation = kf.rotation;
        props.alpha = kf.alpha;
        props.color = kf.color;
        return;
    }

    if (time >= keyframes.back().time) {
        const auto& kf = keyframes.back();
        props.position = kf.position;
        props.scale = kf.scale;
        props.rotation = kf.rotation;
        props.alpha = kf.alpha;
        props.color = kf.color;
        return;
    }

    // Interpolate between keyframes
    const auto& kf0 = keyframes[idx0];
    const auto& kf1 = keyframes[idx1];

    float t = (time - kf0.time) / (kf1.time - kf0.time);

    // Apply easing
    switch (kf0.tweenType) {
        case VintTweenType::EaseIn:
            t = t * t;
            break;
        case VintTweenType::EaseOut:
            t = 1.0f - (1.0f - t) * (1.0f - t);
            break;
        case VintTweenType::EaseInOut:
            t = t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
            break;
        case VintTweenType::Bounce:
            if (t < 1.0f / 2.75f) {
                t = 7.5625f * t * t;
            } else if (t < 2.0f / 2.75f) {
                t -= 1.5f / 2.75f;
                t = 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                t -= 2.25f / 2.75f;
                t = 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                t = 7.5625f * t * t + 0.984375f;
            }
            break;
        case VintTweenType::Elastic:
            if (t == 0 || t == 1) break;
            t = std::pow(2, -10 * t) * std::sin((t - 0.1f) * (2 * 3.14159f) / 0.4f) + 1;
            break;
        default:
            break;
    }

    // Linear interpolation
    props.position.x = kf0.position.x + (kf1.position.x - kf0.position.x) * t;
    props.position.y = kf0.position.y + (kf1.position.y - kf0.position.y) * t;
    props.scale.x = kf0.scale.x + (kf1.scale.x - kf0.scale.x) * t;
    props.scale.y = kf0.scale.y + (kf1.scale.y - kf0.scale.y) * t;
    props.rotation = kf0.rotation + (kf1.rotation - kf0.rotation) * t;
    props.alpha = kf0.alpha + (kf1.alpha - kf0.alpha) * t;
    props.color.r = kf0.color.r + (kf1.color.r - kf0.color.r) * t;
    props.color.g = kf0.color.g + (kf1.color.g - kf0.color.g) * t;
    props.color.b = kf0.color.b + (kf1.color.b - kf0.color.b) * t;
    props.color.a = kf0.color.a + (kf1.color.a - kf0.color.a) * t;
}

// VintElement implementation

VintElement::VintElement(const std::string& name) {
    m_props.name = name;
}

void VintElement::setProperty(const std::string& name, const std::string& value) {
    m_props.customProps[name] = value;

    // Parse known properties
    if (name == "x" || name == "offset_x") {
        m_props.position.x = std::stof(value);
    } else if (name == "y" || name == "offset_y") {
        m_props.position.y = std::stof(value);
    } else if (name == "width" || name == "size_x") {
        m_props.size.x = std::stof(value);
    } else if (name == "height" || name == "size_y") {
        m_props.size.y = std::stof(value);
    } else if (name == "scale_x") {
        m_props.scale.x = std::stof(value);
    } else if (name == "scale_y") {
        m_props.scale.y = std::stof(value);
    } else if (name == "rotation" || name == "angle") {
        m_props.rotation = std::stof(value);
    } else if (name == "alpha" || name == "opacity") {
        m_props.alpha = std::stof(value);
    } else if (name == "visible") {
        m_props.visible = (value == "true" || value == "1");
    } else if (name == "enabled") {
        m_props.enabled = (value == "true" || value == "1");
    } else if (name == "text" || name == "label") {
        m_props.text = value;
    } else if (name == "font") {
        m_props.font = value;
    } else if (name == "font_size") {
        m_props.fontSize = std::stof(value);
    } else if (name == "image" || name == "bitmap") {
        m_props.image = value;
    } else if (name == "h_align") {
        if (value == "left") m_props.hAlign = VintAlignment::Left;
        else if (value == "center") m_props.hAlign = VintAlignment::Center;
        else if (value == "right") m_props.hAlign = VintAlignment::Right;
    } else if (name == "v_align") {
        if (value == "top") m_props.vAlign = VintAlignment::Top;
        else if (value == "middle") m_props.vAlign = VintAlignment::Middle;
        else if (value == "bottom") m_props.vAlign = VintAlignment::Bottom;
    } else if (name == "word_wrap") {
        m_props.wordWrap = (value == "true" || value == "1");
    }
}

std::string VintElement::getProperty(const std::string& name) const {
    auto it = m_props.customProps.find(name);
    return (it != m_props.customProps.end()) ? it->second : "";
}

VintElement* VintElement::addChild(std::unique_ptr<VintElement> child) {
    child->m_parent = this;
    m_children.push_back(std::move(child));
    return m_children.back().get();
}

VintElement* VintElement::addChild(const std::string& name) {
    return addChild(std::make_unique<VintElement>(name));
}

void VintElement::removeChild(VintElement* child) {
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [child](const auto& ptr) { return ptr.get() == child; }),
        m_children.end()
    );
}

VintElement* VintElement::findChild(const std::string& name, bool recursive) const {
    for (const auto& child : m_children) {
        if (child->m_props.name == name) {
            return child.get();
        }
        if (recursive) {
            if (auto found = child->findChild(name, true)) {
                return found;
            }
        }
    }
    return nullptr;
}

VintRect VintElement::getWorldRect() const {
    VintRect rect;
    rect.x = m_props.position.x;
    rect.y = m_props.position.y;
    rect.width = m_props.size.x * m_props.scale.x;
    rect.height = m_props.size.y * m_props.scale.y;

    // Add parent offset
    if (m_parent) {
        VintRect parentRect = m_parent->getWorldRect();
        rect.x += parentRect.x;
        rect.y += parentRect.y;
    }

    return rect;
}

VintColor VintElement::getWorldColor() const {
    VintColor color = m_props.color;
    if (m_parent) {
        VintColor parentColor = m_parent->getWorldColor();
        color.r *= parentColor.r;
        color.g *= parentColor.g;
        color.b *= parentColor.b;
        color.a *= parentColor.a;
    }
    return color;
}

float VintElement::getWorldAlpha() const {
    float alpha = m_props.alpha;
    if (m_parent) {
        alpha *= m_parent->getWorldAlpha();
    }
    return alpha;
}

bool VintElement::isWorldVisible() const {
    if (!m_props.visible) return false;
    if (m_parent) return m_parent->isWorldVisible();
    return true;
}

void VintElement::playAnimation(const std::string& name) {
    // Animation lookup would be done through the document
    m_animTime = 0;
    m_animPlaying = true;
}

void VintElement::stopAnimation() {
    m_animPlaying = false;
    m_currentAnim = nullptr;
}

void VintElement::update(float deltaTime) {
    // Update animation
    if (m_animPlaying && m_currentAnim) {
        m_animTime += deltaTime;
        m_currentAnim->sample(m_animTime, m_props);

        // Check if finished
        if (!m_currentAnim->loop && m_animTime >= m_currentAnim->duration) {
            m_animPlaying = false;
        }
    }

    // Update children
    for (auto& child : m_children) {
        child->update(deltaTime);
    }
}

// VintDocument implementation

VintDocument::VintDocument() {
    m_root = std::make_unique<VintElement>("root");
    m_root->properties().type = VintElementType::Document;
    registerElement(m_root.get());
}

bool VintDocument::loadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open vint_doc: " << path << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    m_name = path.stem().string();
    return loadFromString(buffer.str());
}

bool VintDocument::loadFromMemory(const uint8_t* data, size_t size) {
    std::string xml(reinterpret_cast<const char*>(data), size);
    return loadFromString(xml);
}

bool VintDocument::loadFromString(const std::string& xml) {
    return parseXML(xml);
}

VintElement* VintDocument::findElement(const std::string& name) const {
    return m_root->findChild(name, true);
}

VintElement* VintDocument::findElementByHandle(uint32_t handle) const {
    auto it = m_handleMap.find(handle);
    return (it != m_handleMap.end()) ? it->second : nullptr;
}

void VintDocument::addAnimation(const std::string& name, std::shared_ptr<VintAnimation> anim) {
    m_animations[name] = anim;
}

VintAnimation* VintDocument::getAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    return (it != m_animations.end()) ? it->second.get() : nullptr;
}

void VintDocument::update(float deltaTime) {
    m_root->update(deltaTime);
}

void VintDocument::triggerEvent(VintElement* element, const std::string& event) {
    if (m_eventCallback) {
        m_eventCallback(element, event);
    }
}

void VintDocument::registerElement(VintElement* elem) {
    uint32_t handle = m_nextHandle++;
    elem->setHandle(handle);
    m_handleMap[handle] = elem;
}

// Simple XML parser (minimal implementation for vint_doc format)
// For production, use pugixml or similar library

namespace {

struct XmlNode {
    std::string tag;
    std::unordered_map<std::string, std::string> attributes;
    std::vector<std::unique_ptr<XmlNode>> children;
    std::string text;
};

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string parseAttributeValue(const std::string& xml, size_t& pos) {
    char quote = xml[pos];
    if (quote != '"' && quote != '\'') return "";
    pos++;
    size_t start = pos;
    while (pos < xml.size() && xml[pos] != quote) pos++;
    std::string value = xml.substr(start, pos - start);
    if (pos < xml.size()) pos++; // skip closing quote
    return value;
}

std::unique_ptr<XmlNode> parseXmlNode(const std::string& xml, size_t& pos);

std::vector<std::unique_ptr<XmlNode>> parseXmlChildren(const std::string& xml, size_t& pos, const std::string& parentTag) {
    std::vector<std::unique_ptr<XmlNode>> children;

    while (pos < xml.size()) {
        // Skip whitespace
        while (pos < xml.size() && std::isspace(xml[pos])) pos++;
        if (pos >= xml.size()) break;

        // Check for closing tag
        if (xml[pos] == '<') {
            if (pos + 1 < xml.size() && xml[pos + 1] == '/') {
                // End tag
                size_t endPos = xml.find('>', pos);
                if (endPos != std::string::npos) {
                    pos = endPos + 1;
                }
                break;
            }

            // Check for comment
            if (pos + 4 < xml.size() && xml.substr(pos, 4) == "<!--") {
                size_t endComment = xml.find("-->", pos);
                if (endComment != std::string::npos) {
                    pos = endComment + 3;
                    continue;
                }
            }

            // Parse child node
            auto child = parseXmlNode(xml, pos);
            if (child) {
                children.push_back(std::move(child));
            }
        } else {
            // Text content
            size_t start = pos;
            while (pos < xml.size() && xml[pos] != '<') pos++;
            // Skip text content for now
        }
    }

    return children;
}

std::unique_ptr<XmlNode> parseXmlNode(const std::string& xml, size_t& pos) {
    // Skip to opening <
    while (pos < xml.size() && xml[pos] != '<') pos++;
    if (pos >= xml.size()) return nullptr;
    pos++; // skip <

    // Check for special tags
    if (pos < xml.size() && (xml[pos] == '?' || xml[pos] == '!')) {
        // Skip declaration or comment
        size_t end = xml.find('>', pos);
        if (end != std::string::npos) pos = end + 1;
        return nullptr;
    }

    auto node = std::make_unique<XmlNode>();

    // Parse tag name
    size_t tagStart = pos;
    while (pos < xml.size() && !std::isspace(xml[pos]) && xml[pos] != '>' && xml[pos] != '/') pos++;
    node->tag = xml.substr(tagStart, pos - tagStart);

    // Parse attributes
    while (pos < xml.size() && xml[pos] != '>' && xml[pos] != '/') {
        // Skip whitespace
        while (pos < xml.size() && std::isspace(xml[pos])) pos++;
        if (pos >= xml.size() || xml[pos] == '>' || xml[pos] == '/') break;

        // Parse attribute name
        size_t attrStart = pos;
        while (pos < xml.size() && xml[pos] != '=' && !std::isspace(xml[pos])) pos++;
        std::string attrName = xml.substr(attrStart, pos - attrStart);

        // Skip to =
        while (pos < xml.size() && std::isspace(xml[pos])) pos++;
        if (pos < xml.size() && xml[pos] == '=') {
            pos++;
            while (pos < xml.size() && std::isspace(xml[pos])) pos++;
            std::string attrValue = parseAttributeValue(xml, pos);
            node->attributes[attrName] = attrValue;
        }
    }

    // Check for self-closing tag
    if (pos < xml.size() && xml[pos] == '/') {
        pos++; // skip /
        if (pos < xml.size() && xml[pos] == '>') pos++; // skip >
        return node;
    }

    // Skip >
    if (pos < xml.size() && xml[pos] == '>') pos++;

    // Parse children
    node->children = parseXmlChildren(xml, pos, node->tag);

    return node;
}

} // anonymous namespace

bool VintDocument::parseXML(const std::string& xml) {
    size_t pos = 0;

    // Skip BOM if present
    if (xml.size() >= 3 && (unsigned char)xml[0] == 0xEF &&
        (unsigned char)xml[1] == 0xBB && (unsigned char)xml[2] == 0xBF) {
        pos = 3;
    }

    // Parse root node
    auto rootNode = parseXmlNode(xml, pos);
    if (!rootNode) {
        std::cerr << "Failed to parse vint_doc XML\n";
        return false;
    }

    // Convert to VintElements
    m_root = std::make_unique<VintElement>(rootNode->tag);
    m_root->properties().type = VintElementType::Document;

    // Parse attributes
    for (const auto& [name, value] : rootNode->attributes) {
        m_root->setProperty(name, value);

        if (name == "name") m_name = value;
        else if (name == "design_width") m_designSize.x = std::stof(value);
        else if (name == "design_height") m_designSize.y = std::stof(value);
    }

    registerElement(m_root.get());

    // Parse children recursively
    for (const auto& childNode : rootNode->children) {
        parseElement(childNode.get(), m_root.get());
    }

    std::cout << "Loaded vint_doc: " << m_name << " (" << m_handleMap.size() << " elements)\n";
    return true;
}

VintElement* VintDocument::parseElement(void* nodePtr, VintElement* parent) {
    auto* node = static_cast<XmlNode*>(nodePtr);
    if (!node || node->tag.empty()) return nullptr;

    auto elem = std::make_unique<VintElement>(node->tag);

    // Determine element type from tag
    if (node->tag == "group" || node->tag == "Group") {
        elem->properties().type = VintElementType::Group;
    } else if (node->tag == "object" || node->tag == "Object") {
        elem->properties().type = VintElementType::Object;
    } else if (node->tag == "bitmap" || node->tag == "Bitmap" || node->tag == "image") {
        elem->properties().type = VintElementType::Bitmap;
    } else if (node->tag == "text" || node->tag == "Text" || node->tag == "label") {
        elem->properties().type = VintElementType::Text;
    } else if (node->tag == "animation" || node->tag == "Animation") {
        elem->properties().type = VintElementType::Animation;
    } else if (node->tag == "tween" || node->tag == "Tween") {
        elem->properties().type = VintElementType::Tween;
    } else if (node->tag == "button" || node->tag == "Button") {
        elem->properties().type = VintElementType::Button;
    } else if (node->tag == "list" || node->tag == "List") {
        elem->properties().type = VintElementType::List;
    } else if (node->tag == "grid" || node->tag == "Grid") {
        elem->properties().type = VintElementType::Grid;
    } else if (node->tag == "slider" || node->tag == "Slider") {
        elem->properties().type = VintElementType::Slider;
    } else if (node->tag == "scrollbar" || node->tag == "Scrollbar") {
        elem->properties().type = VintElementType::ScrollBar;
    } else if (node->tag == "meter" || node->tag == "Meter") {
        elem->properties().type = VintElementType::Meter;
    }

    // Parse attributes
    for (const auto& [name, value] : node->attributes) {
        elem->setProperty(name, value);
        if (name == "name") {
            elem->properties().name = value;
        }
    }

    // Add to parent
    VintElement* elemPtr = parent->addChild(std::move(elem));
    registerElement(elemPtr);

    // Parse children
    for (const auto& childNode : node->children) {
        parseElement(childNode.get(), elemPtr);
    }

    return elemPtr;
}

VintAnimation VintDocument::parseAnimation(void* nodePtr) {
    auto* node = static_cast<XmlNode*>(nodePtr);
    VintAnimation anim;

    if (auto it = node->attributes.find("name"); it != node->attributes.end()) {
        anim.name = it->second;
    }
    if (auto it = node->attributes.find("duration"); it != node->attributes.end()) {
        anim.duration = std::stof(it->second);
    }
    if (auto it = node->attributes.find("loop"); it != node->attributes.end()) {
        anim.loop = (it->second == "true" || it->second == "1");
    }

    // Parse keyframes from children
    for (const auto& child : node->children) {
        if (child->tag == "keyframe" || child->tag == "Keyframe") {
            VintKeyframe kf;

            for (const auto& [name, value] : child->attributes) {
                if (name == "time") kf.time = std::stof(value);
                else if (name == "x") kf.position.x = std::stof(value);
                else if (name == "y") kf.position.y = std::stof(value);
                else if (name == "scale_x") kf.scale.x = std::stof(value);
                else if (name == "scale_y") kf.scale.y = std::stof(value);
                else if (name == "rotation") kf.rotation = std::stof(value);
                else if (name == "alpha") kf.alpha = std::stof(value);
                else if (name == "tween") {
                    if (value == "linear") kf.tweenType = VintTweenType::Linear;
                    else if (value == "ease_in") kf.tweenType = VintTweenType::EaseIn;
                    else if (value == "ease_out") kf.tweenType = VintTweenType::EaseOut;
                    else if (value == "ease_in_out") kf.tweenType = VintTweenType::EaseInOut;
                    else if (value == "bounce") kf.tweenType = VintTweenType::Bounce;
                    else if (value == "elastic") kf.tweenType = VintTweenType::Elastic;
                }
            }

            anim.keyframes.push_back(kf);
        }
    }

    return anim;
}

// UISystem implementation

UISystem::UISystem() = default;

UISystem::~UISystem() {
    shutdown();
}

bool UISystem::initialize() {
    if (m_initialized) return true;

    m_screenSize = {1280, 720};
    m_initialized = true;

    std::cout << "UI system initialized\n";
    return true;
}

void UISystem::shutdown() {
    m_documents.clear();
    m_documentStack.clear();
    m_focusedElement = nullptr;
    m_hoveredElement = nullptr;
    m_initialized = false;
}

VintDocument* UISystem::loadDocument(const std::string& name, const std::filesystem::path& path) {
    auto doc = std::make_unique<VintDocument>();
    if (!doc->loadFromFile(path)) {
        return nullptr;
    }

    VintDocument* ptr = doc.get();
    m_documents[name] = std::move(doc);
    return ptr;
}

VintDocument* UISystem::loadDocument(const std::string& name, const uint8_t* data, size_t size) {
    auto doc = std::make_unique<VintDocument>();
    if (!doc->loadFromMemory(data, size)) {
        return nullptr;
    }

    VintDocument* ptr = doc.get();
    m_documents[name] = std::move(doc);
    return ptr;
}

void UISystem::unloadDocument(const std::string& name) {
    // Remove from stack
    m_documentStack.erase(
        std::remove(m_documentStack.begin(), m_documentStack.end(), name),
        m_documentStack.end()
    );

    // Remove document
    m_documents.erase(name);
}

VintDocument* UISystem::getDocument(const std::string& name) const {
    auto it = m_documents.find(name);
    return (it != m_documents.end()) ? it->second.get() : nullptr;
}

void UISystem::pushDocument(const std::string& name) {
    if (m_documents.find(name) == m_documents.end()) {
        std::cerr << "Document not found: " << name << "\n";
        return;
    }
    m_documentStack.push_back(name);
}

void UISystem::popDocument() {
    if (!m_documentStack.empty()) {
        m_documentStack.pop_back();
    }
}

VintDocument* UISystem::activeDocument() const {
    if (m_documentStack.empty()) return nullptr;
    return getDocument(m_documentStack.back());
}

void UISystem::update(float deltaTime) {
    // Update all documents in the stack
    for (const auto& name : m_documentStack) {
        if (auto* doc = getDocument(name)) {
            doc->update(deltaTime);
        }
    }
}

bool UISystem::onMouseMove(float x, float y) {
    m_mousePos = {x, y};

    if (auto* doc = activeDocument()) {
        VintVec2 uiPos = screenToUI(x, y);
        VintElement* hit = hitTest(doc, uiPos.x, uiPos.y);

        if (hit != m_hoveredElement) {
            if (m_hoveredElement) {
                doc->triggerEvent(m_hoveredElement, "mouse_leave");
            }
            m_hoveredElement = hit;
            if (m_hoveredElement) {
                doc->triggerEvent(m_hoveredElement, "mouse_enter");
            }
        }

        return hit != nullptr;
    }

    return false;
}

bool UISystem::onMouseButton(int button, bool pressed) {
    if (auto* doc = activeDocument()) {
        VintVec2 uiPos = screenToUI(m_mousePos.x, m_mousePos.y);
        VintElement* hit = hitTest(doc, uiPos.x, uiPos.y);

        if (hit) {
            if (pressed) {
                m_focusedElement = hit;
                doc->triggerEvent(hit, button == 0 ? "mouse_down" : "right_mouse_down");
            } else {
                doc->triggerEvent(hit, button == 0 ? "mouse_up" : "right_mouse_up");
                if (hit == m_focusedElement) {
                    doc->triggerEvent(hit, button == 0 ? "click" : "right_click");
                }
            }
            return true;
        }
    }

    return false;
}

bool UISystem::onKeyPress(int key, bool pressed) {
    if (auto* doc = activeDocument()) {
        if (m_focusedElement) {
            doc->triggerEvent(m_focusedElement, pressed ? "key_down" : "key_up");
            return true;
        }
    }
    return false;
}

void UISystem::setScreenSize(float width, float height) {
    m_screenSize = {width, height};
}

VintVec2 UISystem::screenToUI(float x, float y) const {
    // Scale from screen coordinates to UI design coordinates
    if (auto* doc = activeDocument()) {
        VintVec2 design = doc->designSize();
        return {
            x * design.x / m_screenSize.x,
            y * design.y / m_screenSize.y
        };
    }
    return {x, y};
}

VintVec2 UISystem::uiToScreen(float x, float y) const {
    if (auto* doc = activeDocument()) {
        VintVec2 design = doc->designSize();
        return {
            x * m_screenSize.x / design.x,
            y * m_screenSize.y / design.y
        };
    }
    return {x, y};
}

VintElement* UISystem::hitTest(VintDocument* doc, float x, float y) {
    if (!doc || !doc->root()) return nullptr;

    // Reverse traverse to get topmost element
    std::function<VintElement*(VintElement*)> testElement = [&](VintElement* elem) -> VintElement* {
        if (!elem->isWorldVisible() || !elem->properties().enabled) {
            return nullptr;
        }

        // Test children first (reverse order for top-most)
        const auto& children = elem->children();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (VintElement* hit = testElement(it->get())) {
                return hit;
            }
        }

        // Test this element
        VintRect rect = elem->getWorldRect();
        if (rect.contains(x, y)) {
            return elem;
        }

        return nullptr;
    };

    return testElement(doc->root());
}

// Global UI system singleton
UISystem& getUISystem() {
    static UISystem instance;
    return instance;
}

} // namespace opensaints
