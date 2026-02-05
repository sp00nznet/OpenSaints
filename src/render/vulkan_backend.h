#pragma once
// Vulkan rendering backend for OpenSaints

#include "renderer.h"
#include <vector>
#include <unordered_map>

#ifdef HAVE_VULKAN

// Vulkan forward declarations
typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkSwapchainKHR_T* VkSwapchainKHR;
typedef struct VkRenderPass_T* VkRenderPass;
typedef struct VkCommandPool_T* VkCommandPool;
typedef struct VkCommandBuffer_T* VkCommandBuffer;
typedef struct VkSemaphore_T* VkSemaphore;
typedef struct VkFence_T* VkFence;
typedef struct VkFramebuffer_T* VkFramebuffer;
typedef struct VkImageView_T* VkImageView;
typedef struct VkImage_T* VkImage;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkShaderModule_T* VkShaderModule;
typedef struct VkPipelineLayout_T* VkPipelineLayout;
typedef struct VkPipeline_T* VkPipeline;
typedef struct VkDescriptorSetLayout_T* VkDescriptorSetLayout;
typedef struct VkDescriptorPool_T* VkDescriptorPool;
typedef struct VkDescriptorSet_T* VkDescriptorSet;
typedef struct VkSampler_T* VkSampler;

namespace opensaints {

// Maximum frames in flight
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// Internal buffer wrapper
struct VulkanBuffer {
    VkBuffer buffer = nullptr;
    VkDeviceMemory memory = nullptr;
    size_t size = 0;
    BufferType type;
};

// Internal texture wrapper
struct VulkanTexture {
    VkImage image = nullptr;
    VkDeviceMemory memory = nullptr;
    VkImageView view = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format;
};

// Internal shader wrapper
struct VulkanShader {
    VkShaderModule module = nullptr;
    ShaderStage stage;
};

// Internal pipeline wrapper
struct VulkanPipeline {
    VkPipeline pipeline = nullptr;
    VkPipelineLayout layout = nullptr;
};

// Vulkan renderer implementation
class VulkanRenderer : public Renderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    // Renderer interface
    bool initialize(Application* app) override;
    void shutdown() override;
    bool isInitialized() const override { return m_initialized; }

    bool beginFrame() override;
    void endFrame() override;

    void onResize(int width, int height) override;
    void clear(const Color& color) override;

    BufferHandle createBuffer(BufferType type, const void* data, size_t size) override;
    void updateBuffer(BufferHandle buffer, const void* data, size_t size, size_t offset) override;
    void destroyBuffer(BufferHandle buffer) override;

    TextureHandle createTexture(uint32_t width, uint32_t height,
                                TextureFormat format, const void* data) override;
    void destroyTexture(TextureHandle texture) override;

    ShaderHandle createShader(ShaderStage stage, const uint8_t* code, size_t size) override;
    void destroyShader(ShaderHandle shader) override;

    PipelineHandle createPipeline(const PipelineDesc& desc) override;
    void destroyPipeline(PipelineHandle pipeline) override;

    void bindPipeline(PipelineHandle pipeline) override;
    void bindVertexBuffer(BufferHandle buffer) override;
    void bindIndexBuffer(BufferHandle buffer) override;
    void bindTexture(uint32_t slot, TextureHandle texture) override;
    void setUniforms(const UniformData& uniforms) override;
    void draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;

    RenderStats getStats() const override;
    RenderCaps getCaps() const override;

private:
    Application* m_app = nullptr;
    bool m_initialized = false;
    bool m_framebufferResized = false;

    // Vulkan core objects
    VkInstance m_instance = nullptr;
    VkPhysicalDevice m_physicalDevice = nullptr;
    VkDevice m_device = nullptr;
    VkQueue m_graphicsQueue = nullptr;
    VkQueue m_presentQueue = nullptr;
    VkSurfaceKHR m_surface = nullptr;

    // Queue family indices
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;

    // Swapchain
    VkSwapchainKHR m_swapchain = nullptr;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    uint32_t m_swapchainImageCount = 0;
    uint32_t m_swapchainWidth = 0;
    uint32_t m_swapchainHeight = 0;

    // Depth buffer
    VkImage m_depthImage = nullptr;
    VkDeviceMemory m_depthImageMemory = nullptr;
    VkImageView m_depthImageView = nullptr;
    uint32_t m_swapchainFormat = 50; // VK_FORMAT_B8G8R8A8_SRGB

    // Render pass and framebuffers
    VkRenderPass m_renderPass = nullptr;
    std::vector<VkFramebuffer> m_framebuffers;

    // Command pools and buffers
    VkCommandPool m_commandPool = nullptr;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Synchronization
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;

    // Descriptors
    VkDescriptorSetLayout m_descriptorSetLayout = nullptr;
    VkDescriptorPool m_descriptorPool = nullptr;
    std::vector<VkDescriptorSet> m_descriptorSets;

    // Default resources
    VkSampler m_defaultSampler = nullptr;
    TextureHandle m_whiteTexture = InvalidTexture;
    PipelineHandle m_defaultPipeline = InvalidPipeline;
    ShaderHandle m_defaultVertShader = InvalidShader;
    ShaderHandle m_defaultFragShader = InvalidShader;

    // Uniform buffers (per frame)
    std::vector<VulkanBuffer> m_uniformBuffers;

    // Resource management
    std::unordered_map<BufferHandle, VulkanBuffer> m_buffers;
    std::unordered_map<TextureHandle, VulkanTexture> m_textures;
    std::unordered_map<ShaderHandle, VulkanShader> m_shaders;
    std::unordered_map<PipelineHandle, VulkanPipeline> m_pipelines;
    uint64_t m_nextHandle = 1;

    // Current state
    PipelineHandle m_boundPipeline = InvalidPipeline;
    BufferHandle m_boundVertexBuffer = InvalidBuffer;
    BufferHandle m_boundIndexBuffer = InvalidBuffer;
    TextureHandle m_boundTextures[8] = {InvalidTexture};
    Color m_clearColor{0.392f, 0.584f, 0.929f, 1.0f}; // Cornflower blue default

    // Stats
    mutable RenderStats m_stats = {};
    mutable RenderCaps m_caps = {};

    // Initialization helpers
    bool createInstance();
    bool createSurface();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createDescriptorSetLayout();
    bool createDescriptorPool();
    bool createUniformBuffers();
    bool createDefaultResources();

    // Cleanup helpers
    void cleanupSwapchain();
    void recreateSwapchain();

    // Buffer helpers
    uint32_t findMemoryType(uint32_t typeFilter, uint32_t properties);
    bool createVkBuffer(size_t size, uint32_t usage, uint32_t properties,
                        VkBuffer& buffer, VkDeviceMemory& memory);
    void copyBuffer(VkBuffer src, VkBuffer dst, size_t size);

    // Image helpers
    bool createImage(uint32_t width, uint32_t height, uint32_t format,
                     uint32_t tiling, uint32_t usage, uint32_t properties,
                     VkImage& image, VkDeviceMemory& memory);
    VkImageView createImageView(VkImage image, uint32_t format, uint32_t aspectFlags);
    void transitionImageLayout(VkImage image, uint32_t format,
                               uint32_t oldLayout, uint32_t newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // Command buffer helpers
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
};

} // namespace opensaints

#endif // HAVE_VULKAN
