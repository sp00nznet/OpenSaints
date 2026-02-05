#pragma once
// VINT Document Parser for OpenSaints
// Parses Saints Row 2 UI definition files (.vint_doc)

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <filesystem>

namespace opensaints {

// Forward declarations
class VintElement;
class VintDocument;

// UI element types
enum class VintElementType {
    Unknown,
    Document,
    Group,
    Object,
    Bitmap,
    Text,
    Animation,
    Tween,
    Event,
    Grid,
    GridCell,
    List,
    ListItem,
    Button,
    Slider,
    ScrollBar,
    Meter,
    Marquee
};

// Text alignment
enum class VintAlignment {
    Left,
    Center,
    Right,
    Top,
    Middle,
    Bottom
};

// Anchor points
enum class VintAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

// Tween types for animations
enum class VintTweenType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bounce,
    Elastic,
    Custom
};

// Color (RGBA)
struct VintColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    VintColor() = default;
    VintColor(float r, float g, float b, float a = 1.0f)
        : r(r), g(g), b(b), a(a) {}

    static VintColor fromHex(uint32_t hex);
    static VintColor white() { return VintColor(1, 1, 1, 1); }
    static VintColor black() { return VintColor(0, 0, 0, 1); }
    static VintColor transparent() { return VintColor(0, 0, 0, 0); }
};

// Rectangle
struct VintRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    bool contains(float px, float py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

// 2D Vector
struct VintVec2 {
    float x = 0;
    float y = 0;

    VintVec2() = default;
    VintVec2(float x, float y) : x(x), y(y) {}
};

// Element properties
struct VintProperties {
    std::string name;
    VintElementType type = VintElementType::Unknown;

    // Transform
    VintVec2 position;
    VintVec2 size;
    VintVec2 scale = {1, 1};
    float rotation = 0;
    VintAnchor anchor = VintAnchor::TopLeft;

    // Appearance
    VintColor color = VintColor::white();
    float alpha = 1.0f;
    bool visible = true;
    bool enabled = true;

    // Text-specific
    std::string text;
    std::string font;
    float fontSize = 14.0f;
    VintAlignment hAlign = VintAlignment::Left;
    VintAlignment vAlign = VintAlignment::Top;
    bool wordWrap = false;

    // Bitmap-specific
    std::string image;
    VintRect uvRect = {0, 0, 1, 1};

    // Animation
    std::string animationName;
    bool autoPlay = false;
    bool loop = false;

    // Custom properties
    std::unordered_map<std::string, std::string> customProps;
};

// Animation keyframe
struct VintKeyframe {
    float time = 0;
    VintVec2 position;
    VintVec2 scale = {1, 1};
    float rotation = 0;
    float alpha = 1.0f;
    VintColor color = VintColor::white();
    VintTweenType tweenType = VintTweenType::Linear;
};

// Animation definition
struct VintAnimation {
    std::string name;
    float duration = 0;
    bool loop = false;
    std::vector<VintKeyframe> keyframes;

    void sample(float time, VintProperties& props) const;
};

// UI Element in the hierarchy
class VintElement {
public:
    VintElement() = default;
    explicit VintElement(const std::string& name);
    ~VintElement() = default;

    // Properties
    VintProperties& properties() { return m_props; }
    const VintProperties& properties() const { return m_props; }

    void setProperty(const std::string& name, const std::string& value);
    std::string getProperty(const std::string& name) const;

    // Hierarchy
    VintElement* parent() const { return m_parent; }
    const std::vector<std::unique_ptr<VintElement>>& children() const { return m_children; }

    VintElement* addChild(std::unique_ptr<VintElement> child);
    VintElement* addChild(const std::string& name);
    void removeChild(VintElement* child);
    VintElement* findChild(const std::string& name, bool recursive = true) const;

    // Computed values
    VintRect getWorldRect() const;
    VintColor getWorldColor() const;
    float getWorldAlpha() const;
    bool isWorldVisible() const;

    // Animation
    void playAnimation(const std::string& name);
    void stopAnimation();
    void update(float deltaTime);

    // Handle identifier (for scripting)
    uint32_t handle() const { return m_handle; }
    void setHandle(uint32_t h) { m_handle = h; }

private:
    VintProperties m_props;
    VintElement* m_parent = nullptr;
    std::vector<std::unique_ptr<VintElement>> m_children;
    uint32_t m_handle = 0;

    // Animation state
    const VintAnimation* m_currentAnim = nullptr;
    float m_animTime = 0;
    bool m_animPlaying = false;
};

// Event callback
using VintEventCallback = std::function<void(VintElement*, const std::string& event)>;

// VINT Document (a complete UI screen)
class VintDocument {
public:
    VintDocument();
    ~VintDocument() = default;

    // Loading
    bool loadFromFile(const std::filesystem::path& path);
    bool loadFromMemory(const uint8_t* data, size_t size);
    bool loadFromString(const std::string& xml);

    // Element access
    VintElement* root() { return m_root.get(); }
    const VintElement* root() const { return m_root.get(); }

    VintElement* findElement(const std::string& name) const;
    VintElement* findElementByHandle(uint32_t handle) const;

    // Animations
    void addAnimation(const std::string& name, std::shared_ptr<VintAnimation> anim);
    VintAnimation* getAnimation(const std::string& name) const;

    // Update
    void update(float deltaTime);

    // Events
    void setEventCallback(VintEventCallback callback) { m_eventCallback = callback; }
    void triggerEvent(VintElement* element, const std::string& event);

    // Document info
    const std::string& name() const { return m_name; }
    const VintVec2& designSize() const { return m_designSize; }

private:
    std::string m_name;
    VintVec2 m_designSize = {1280, 720};
    std::unique_ptr<VintElement> m_root;
    std::unordered_map<std::string, std::shared_ptr<VintAnimation>> m_animations;
    std::unordered_map<uint32_t, VintElement*> m_handleMap;
    VintEventCallback m_eventCallback;
    uint32_t m_nextHandle = 1;

    bool parseXML(const std::string& xml);
    VintElement* parseElement(void* node, VintElement* parent);
    VintAnimation parseAnimation(void* node);
    void registerElement(VintElement* elem);
};

// UI System (manages all documents)
class UISystem {
public:
    UISystem();
    ~UISystem();

    // Initialize
    bool initialize();
    void shutdown();

    // Document management
    VintDocument* loadDocument(const std::string& name, const std::filesystem::path& path);
    VintDocument* loadDocument(const std::string& name, const uint8_t* data, size_t size);
    void unloadDocument(const std::string& name);
    VintDocument* getDocument(const std::string& name) const;

    // Active document stack (for menus)
    void pushDocument(const std::string& name);
    void popDocument();
    VintDocument* activeDocument() const;

    // Update all active documents
    void update(float deltaTime);

    // Input events (returns true if consumed)
    bool onMouseMove(float x, float y);
    bool onMouseButton(int button, bool pressed);
    bool onKeyPress(int key, bool pressed);

    // Screen coordinates
    void setScreenSize(float width, float height);
    VintVec2 screenToUI(float x, float y) const;
    VintVec2 uiToScreen(float x, float y) const;

    // Global event callback
    void setGlobalEventCallback(VintEventCallback callback) { m_globalCallback = callback; }

private:
    std::unordered_map<std::string, std::unique_ptr<VintDocument>> m_documents;
    std::vector<std::string> m_documentStack;
    VintVec2 m_screenSize = {1280, 720};
    VintEventCallback m_globalCallback;
    bool m_initialized = false;

    VintElement* m_focusedElement = nullptr;
    VintElement* m_hoveredElement = nullptr;
    VintVec2 m_mousePos;

    VintElement* hitTest(VintDocument* doc, float x, float y);
};

// Global UI system access
UISystem& getUISystem();

} // namespace opensaints
