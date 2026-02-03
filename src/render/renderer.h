#pragma once
// Renderer abstraction for OpenSaints
// Provides a common interface for different rendering backends

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <array>

namespace opensaints {

// Forward declarations
class Application;
struct MeshData;

// Color type
struct Color {
    float r, g, b, a;

    Color() : r(0), g(0), b(0), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static Color white() { return Color(1, 1, 1, 1); }
    static Color black() { return Color(0, 0, 0, 1); }
    static Color red() { return Color(1, 0, 0, 1); }
    static Color green() { return Color(0, 1, 0, 1); }
    static Color blue() { return Color(0, 0, 1, 1); }
    static Color cornflowerBlue() { return Color(0.392f, 0.584f, 0.929f, 1); }
};

// Vertex format for rendering
struct RenderVertex {
    float position[3];
    float normal[3];
    float texcoord[2];
    uint32_t color;
};

// GPU buffer handle
using BufferHandle = uint64_t;
using TextureHandle = uint64_t;
using ShaderHandle = uint64_t;
using PipelineHandle = uint64_t;

constexpr BufferHandle InvalidBuffer = 0;
constexpr TextureHandle InvalidTexture = 0;
constexpr ShaderHandle InvalidShader = 0;
constexpr PipelineHandle InvalidPipeline = 0;

// Buffer types
enum class BufferType {
    Vertex,
    Index,
    Uniform
};

// Texture formats
enum class TextureFormat {
    RGBA8,
    BGRA8,
    RGB8,
    R8,
    Depth24Stencil8,
    BC1,  // DXT1
    BC2,  // DXT3
    BC3   // DXT5
};

// Shader stage
enum class ShaderStage {
    Vertex,
    Fragment
};

// Primitive topology
enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList
};

// Cull mode
enum class CullMode {
    None,
    Front,
    Back
};

// Blend mode
enum class BlendMode {
    Opaque,
    Alpha,
    Additive
};

// Pipeline state description
struct PipelineDesc {
    ShaderHandle vertexShader = InvalidShader;
    ShaderHandle fragmentShader = InvalidShader;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cullMode = CullMode::Back;
    BlendMode blendMode = BlendMode::Opaque;
    bool depthTest = true;
    bool depthWrite = true;
};

// Uniform data for basic rendering
struct UniformData {
    float modelMatrix[16];
    float viewMatrix[16];
    float projectionMatrix[16];
    float cameraPosition[4];
    float lightDirection[4];
    float lightColor[4];
    float ambientColor[4];
};

// Renderer statistics
struct RenderStats {
    uint32_t drawCalls;
    uint32_t triangles;
    uint32_t vertices;
    size_t bufferMemory;
    size_t textureMemory;
    float frameTime;
};

// Renderer capabilities
struct RenderCaps {
    std::string deviceName;
    std::string apiVersion;
    size_t maxTextureSize;
    size_t maxUniformBufferSize;
    bool supportsCompressedTextures;
    bool supportsAnisotropicFiltering;
    bool supportsMultisample;
};

// Base renderer interface
class Renderer {
public:
    virtual ~Renderer() = default;

    // Lifecycle
    virtual bool initialize(Application* app) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;

    // Frame management
    virtual bool beginFrame() = 0;
    virtual void endFrame() = 0;

    // Resize handling
    virtual void onResize(int width, int height) = 0;

    // Clear
    virtual void clear(const Color& color) = 0;

    // Buffer management
    virtual BufferHandle createBuffer(BufferType type, const void* data, size_t size) = 0;
    virtual void updateBuffer(BufferHandle buffer, const void* data, size_t size, size_t offset = 0) = 0;
    virtual void destroyBuffer(BufferHandle buffer) = 0;

    // Texture management
    virtual TextureHandle createTexture(uint32_t width, uint32_t height,
                                        TextureFormat format, const void* data) = 0;
    virtual void destroyTexture(TextureHandle texture) = 0;

    // Shader management
    virtual ShaderHandle createShader(ShaderStage stage, const uint8_t* code, size_t size) = 0;
    virtual void destroyShader(ShaderHandle shader) = 0;

    // Pipeline management
    virtual PipelineHandle createPipeline(const PipelineDesc& desc) = 0;
    virtual void destroyPipeline(PipelineHandle pipeline) = 0;

    // Drawing
    virtual void bindPipeline(PipelineHandle pipeline) = 0;
    virtual void bindVertexBuffer(BufferHandle buffer) = 0;
    virtual void bindIndexBuffer(BufferHandle buffer) = 0;
    virtual void bindTexture(uint32_t slot, TextureHandle texture) = 0;
    virtual void setUniforms(const UniformData& uniforms) = 0;
    virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;

    // Stats and info
    virtual RenderStats getStats() const = 0;
    virtual RenderCaps getCaps() const = 0;

    // Factory
    static std::unique_ptr<Renderer> create();
};

// Math helpers for renderer
namespace RenderMath {
    void identity(float* m);
    void perspective(float* m, float fov, float aspect, float near, float far);
    void lookAt(float* m, const float* eye, const float* target, const float* up);
    void translate(float* m, float x, float y, float z);
    void rotateY(float* m, float angle);
    void multiply(float* result, const float* a, const float* b);
}

} // namespace opensaints
