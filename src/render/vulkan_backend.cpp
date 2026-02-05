#include "vulkan_backend.h"
#include "default_shaders.h"

#ifdef HAVE_VULKAN

#include "../platform/application.h"
#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <set>

namespace opensaints {

// Factory function
std::unique_ptr<Renderer> Renderer::create() {
    return std::make_unique<VulkanRenderer>();
}

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

bool VulkanRenderer::initialize(Application* app) {
    if (m_initialized) {
        return true;
    }

    m_app = app;
    m_swapchainWidth = app->windowWidth();
    m_swapchainHeight = app->windowHeight();

    // Initialize volk (Vulkan loader)
    if (volkInitialize() != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan loader\n";
        return false;
    }

    if (!createInstance()) return false;
    if (!createSurface()) return false;
    if (!selectPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;
    if (!createSwapchain()) return false;
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createCommandPool()) return false;
    if (!createCommandBuffers()) return false;
    if (!createSyncObjects()) return false;
    if (!createDescriptorSetLayout()) return false;
    if (!createDescriptorPool()) return false;
    if (!createUniformBuffers()) return false;
    if (!createDefaultResources()) return false;

    m_initialized = true;
    std::cout << "Vulkan renderer initialized\n";
    std::cout << "  Swapchain: " << m_swapchainWidth << "x" << m_swapchainHeight << "\n";
    std::cout << "  Format: " << m_swapchainFormat << "\n";
    std::cout << "  Images: " << m_swapchainImageCount << "\n";
    std::cout.flush();

    return true;
}

void VulkanRenderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    vkDeviceWaitIdle(m_device);

    // Cleanup resources
    for (auto& [handle, buffer] : m_buffers) {
        vkDestroyBuffer(m_device, buffer.buffer, nullptr);
        vkFreeMemory(m_device, buffer.memory, nullptr);
    }
    m_buffers.clear();

    for (auto& [handle, texture] : m_textures) {
        vkDestroyImageView(m_device, texture.view, nullptr);
        vkDestroyImage(m_device, texture.image, nullptr);
        vkFreeMemory(m_device, texture.memory, nullptr);
    }
    m_textures.clear();

    for (auto& [handle, shader] : m_shaders) {
        vkDestroyShaderModule(m_device, shader.module, nullptr);
    }
    m_shaders.clear();

    for (auto& [handle, pipeline] : m_pipelines) {
        vkDestroyPipeline(m_device, pipeline.pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, pipeline.layout, nullptr);
    }
    m_pipelines.clear();

    for (auto& ub : m_uniformBuffers) {
        vkDestroyBuffer(m_device, ub.buffer, nullptr);
        vkFreeMemory(m_device, ub.memory, nullptr);
    }
    m_uniformBuffers.clear();

    if (m_defaultSampler) {
        vkDestroySampler(m_device, m_defaultSampler, nullptr);
    }

    if (m_descriptorPool) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    }

    if (m_descriptorSetLayout) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    }

    cleanupSwapchain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    if (m_commandPool) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }

    if (m_device) {
        vkDestroyDevice(m_device, nullptr);
    }

    if (m_surface) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }

    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
    }

    m_initialized = false;
    std::cout << "Vulkan renderer shutdown\n";
}

bool VulkanRenderer::createInstance() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "OpenSaints";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "OpenSaints";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    // Get SDL required extensions
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(m_app->getSDLWindow(), &extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(m_app->getSDLWindow(), &extensionCount, extensions.data());

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef _DEBUG
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
#endif

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance\n";
        return false;
    }

    // Load instance-level Vulkan functions
    volkLoadInstance(m_instance);

    return true;
}

bool VulkanRenderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(m_app->getSDLWindow(), m_instance, &m_surface)) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << "\n";
        return false;
    }
    return true;
}

bool VulkanRenderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        std::cerr << "No Vulkan-capable GPUs found\n";
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Select first suitable device
    for (const auto& device : devices) {
        // Check queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        bool hasGraphics = false;
        bool hasPresent = false;

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_graphicsFamily = i;
                hasGraphics = true;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (presentSupport) {
                m_presentFamily = i;
                hasPresent = true;
            }

            if (hasGraphics && hasPresent) break;
        }

        if (hasGraphics && hasPresent) {
            m_physicalDevice = device;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            m_caps.deviceName = props.deviceName;
            std::cout << "Selected GPU: " << props.deviceName << "\n";

            return true;
        }
    }

    std::cerr << "No suitable GPU found\n";
    return false;
}

bool VulkanRenderer::createLogicalDevice() {
    std::set<uint32_t> uniqueQueueFamilies = {m_graphicsFamily, m_presentFamily};

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device\n";
        return false;
    }

    // Load device-level Vulkan functions
    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);

    return true;
}

bool VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    // Choose format
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // Choose present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync

    // Choose extent
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::clamp(m_swapchainWidth, capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(m_swapchainHeight, capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {m_graphicsFamily, m_presentFamily};
    if (m_graphicsFamily != m_presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain\n";
        return false;
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, nullptr);
    m_swapchainImages.resize(m_swapchainImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, m_swapchainImages.data());

    m_swapchainWidth = extent.width;
    m_swapchainHeight = extent.height;
    m_swapchainFormat = surfaceFormat.format;

    // Create image views
    m_swapchainImageViews.resize(m_swapchainImageCount);
    for (uint32_t i = 0; i < m_swapchainImageCount; ++i) {
        m_swapchainImageViews[i] = createImageView(m_swapchainImages[i],
                                                   surfaceFormat.format,
                                                   VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // Create depth buffer
    createImage(m_swapchainWidth, m_swapchainHeight,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_depthImage, m_depthImageMemory);
    m_depthImageView = createImageView(m_depthImage, VK_FORMAT_D24_UNORM_S8_UINT,
                                       VK_IMAGE_ASPECT_DEPTH_BIT);

    return true;
}

bool VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = static_cast<VkFormat>(m_swapchainFormat);
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass\n";
        return false;
    }

    return true;
}

bool VulkanRenderer::createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageCount);

    for (uint32_t i = 0; i < m_swapchainImageCount; ++i) {
        VkImageView attachments[] = {m_swapchainImageViews[i], m_depthImageView};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapchainWidth;
        framebufferInfo.height = m_swapchainHeight;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer\n";
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_graphicsFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool\n";
        return false;
    }

    return true;
}

bool VulkanRenderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers\n";
        return false;
    }

    return true;
}

bool VulkanRenderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create sync objects\n";
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding = {};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerBinding = {};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {uboBinding, samplerBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout\n";
        return false;
    }

    return true;
}

bool VulkanRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT * 10},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 10}
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 10;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool\n";
        return false;
    }

    return true;
}

bool VulkanRenderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformData);

    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor sets\n";
        return false;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        createVkBuffer(bufferSize,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       m_uniformBuffers[i].buffer,
                       m_uniformBuffers[i].memory);
        m_uniformBuffers[i].size = bufferSize;
        m_uniformBuffers[i].type = BufferType::Uniform;

        // Write descriptor set
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = m_uniformBuffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformData);

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }

    return true;
}

bool VulkanRenderer::createDefaultResources() {
    // Create default sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_defaultSampler) != VK_SUCCESS) {
        std::cerr << "Failed to create sampler\n";
        return false;
    }

    // Create 1x1 white texture
    uint32_t whitePixel = 0xFFFFFFFF;
    m_whiteTexture = createTexture(1, 1, TextureFormat::RGBA8, &whitePixel);

    // Create default shaders
    m_defaultVertShader = createShader(ShaderStage::Vertex, default_vert_spv, default_vert_spv_size);
    m_defaultFragShader = createShader(ShaderStage::Fragment, default_frag_spv, default_frag_spv_size);

    if (m_defaultVertShader == InvalidShader || m_defaultFragShader == InvalidShader) {
        std::cerr << "Failed to create default shaders\n";
        return false;
    }

    // Create default pipeline
    PipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = m_defaultVertShader;
    pipelineDesc.fragmentShader = m_defaultFragShader;
    pipelineDesc.depthTest = true;
    pipelineDesc.depthWrite = true;
    pipelineDesc.cullMode = CullMode::Back;

    m_defaultPipeline = createPipeline(pipelineDesc);
    if (m_defaultPipeline == InvalidPipeline) {
        std::cerr << "Failed to create default pipeline\n";
        return false;
    }

    std::cout << "Default pipeline created\n";
    return true;
}

void VulkanRenderer::cleanupSwapchain() {
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);

    for (auto framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_framebuffers.clear();

    for (auto imageView : m_swapchainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapchainImageViews.clear();

    if (m_swapchain) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = nullptr;
    }

    if (m_renderPass) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = nullptr;
    }
}

void VulkanRenderer::recreateSwapchain() {
    vkDeviceWaitIdle(m_device);

    cleanupSwapchain();

    createSwapchain();
    createRenderPass();
    createFramebuffers();
}

void VulkanRenderer::onResize(int width, int height) {
    m_swapchainWidth = width;
    m_swapchainHeight = height;
    m_framebufferResized = true;
}

bool VulkanRenderer::beginFrame() {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                            m_imageAvailableSemaphores[m_currentFrame],
                                            VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return false;
    }

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[m_imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_swapchainWidth, m_swapchainHeight};

    VkClearValue clearValues[2] = {};
    clearValues[0].color.float32[0] = m_clearColor.r;
    clearValues[0].color.float32[1] = m_clearColor.g;
    clearValues[0].color.float32[2] = m_clearColor.b;
    clearValues[0].color.float32[3] = m_clearColor.a;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(m_swapchainWidth);
    viewport.height = static_cast<float>(m_swapchainHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {m_swapchainWidth, m_swapchainHeight};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_stats.drawCalls = 0;
    m_stats.triangles = 0;
    m_stats.vertices = 0;

    // Reset bound state for new frame (command buffer was reset)
    m_boundPipeline = InvalidPipeline;
    m_boundVertexBuffer = InvalidBuffer;
    m_boundIndexBuffer = InvalidBuffer;

    return true;
}

void VulkanRenderer::endFrame() {
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    static int frameCount = 0;
    if (++frameCount <= 3) {
        std::cout << "Frame " << frameCount << " presented (result: " << result << ")\n";
        std::cout.flush();
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::clear(const Color& color) {
    m_clearColor = color;
}

// Buffer, texture, shader, pipeline implementations (simplified stubs)

BufferHandle VulkanRenderer::createBuffer(BufferType type, const void* data, size_t size) {
    VulkanBuffer buffer;
    buffer.type = type;
    buffer.size = size;

    VkBufferUsageFlags usage;
    switch (type) {
        case BufferType::Vertex: usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
        case BufferType::Index: usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
        case BufferType::Uniform: usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
    }

    // Create staging buffer and copy
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createVkBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(m_device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(m_device, stagingMemory);

    createVkBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   buffer.buffer, buffer.memory);

    copyBuffer(stagingBuffer, buffer.buffer, size);

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    BufferHandle handle = m_nextHandle++;
    m_buffers[handle] = buffer;
    return handle;
}

void VulkanRenderer::updateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) return;

    // For simplicity, recreate buffer (real impl would use staging)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createVkBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(m_device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(m_device, stagingMemory);

    copyBuffer(stagingBuffer, it->second.buffer, size);

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);
}

void VulkanRenderer::destroyBuffer(BufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it != m_buffers.end()) {
        vkDestroyBuffer(m_device, it->second.buffer, nullptr);
        vkFreeMemory(m_device, it->second.memory, nullptr);
        m_buffers.erase(it);
    }
}

TextureHandle VulkanRenderer::createTexture(uint32_t width, uint32_t height,
                                            TextureFormat format, const void* data) {
    VulkanTexture texture;
    texture.width = width;
    texture.height = height;
    texture.format = format;

    VkDeviceSize imageSize = width * height * 4;

    // Staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createVkBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &mapped);
    memcpy(mapped, data, imageSize);
    vkUnmapMemory(m_device, stagingMemory);

    createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                texture.image, texture.memory);

    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture.image, width, height);
    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    texture.view = createImageView(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    TextureHandle handle = m_nextHandle++;
    m_textures[handle] = texture;
    return handle;
}

void VulkanRenderer::destroyTexture(TextureHandle handle) {
    auto it = m_textures.find(handle);
    if (it != m_textures.end()) {
        vkDestroyImageView(m_device, it->second.view, nullptr);
        vkDestroyImage(m_device, it->second.image, nullptr);
        vkFreeMemory(m_device, it->second.memory, nullptr);
        m_textures.erase(it);
    }
}

ShaderHandle VulkanRenderer::createShader(ShaderStage stage, const uint8_t* code, size_t size) {
    VulkanShader shader;
    shader.stage = stage;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code);

    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shader.module) != VK_SUCCESS) {
        return InvalidShader;
    }

    ShaderHandle handle = m_nextHandle++;
    m_shaders[handle] = shader;
    return handle;
}

void VulkanRenderer::destroyShader(ShaderHandle handle) {
    auto it = m_shaders.find(handle);
    if (it != m_shaders.end()) {
        vkDestroyShaderModule(m_device, it->second.module, nullptr);
        m_shaders.erase(it);
    }
}

PipelineHandle VulkanRenderer::createPipeline(const PipelineDesc& desc) {
    auto vsIt = m_shaders.find(desc.vertexShader);
    auto fsIt = m_shaders.find(desc.fragmentShader);
    if (vsIt == m_shaders.end() || fsIt == m_shaders.end()) {
        return InvalidPipeline;
    }

    VulkanPipeline pipeline;

    // Pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;

    vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipeline.layout);

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vsIt->second.module;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fsIt->second.module;
    shaderStages[1].pName = "main";

    // Vertex input
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(RenderVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[4] = {};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = offsetof(RenderVertex, position);

    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(RenderVertex, normal);

    attrDescs[2].binding = 0;
    attrDescs[2].location = 2;
    attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[2].offset = offsetof(RenderVertex, texcoord);

    attrDescs[3].binding = 0;
    attrDescs[3].location = 3;
    attrDescs[3].format = VK_FORMAT_R8G8B8A8_UNORM;
    attrDescs[3].offset = offsetof(RenderVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 4;
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = desc.cullMode == CullMode::None ? VK_CULL_MODE_NONE :
                          desc.cullMode == CullMode::Front ? VK_CULL_MODE_FRONT_BIT :
                          VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (desc.blendMode == BlendMode::Alpha) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline.layout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(m_device, pipeline.layout, nullptr);
        return InvalidPipeline;
    }

    PipelineHandle handle = m_nextHandle++;
    m_pipelines[handle] = pipeline;
    return handle;
}

void VulkanRenderer::destroyPipeline(PipelineHandle handle) {
    auto it = m_pipelines.find(handle);
    if (it != m_pipelines.end()) {
        vkDestroyPipeline(m_device, it->second.pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, it->second.layout, nullptr);
        m_pipelines.erase(it);
    }
}

void VulkanRenderer::bindPipeline(PipelineHandle handle) {
    auto it = m_pipelines.find(handle);
    if (it != m_pipelines.end()) {
        vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, it->second.pipeline);
        // Bind descriptor set with uniform buffer
        vkCmdBindDescriptorSets(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                it->second.layout, 0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);
        m_boundPipeline = handle;
    }
}

void VulkanRenderer::bindVertexBuffer(BufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it != m_buffers.end()) {
        VkBuffer buffers[] = {it->second.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(m_commandBuffers[m_currentFrame], 0, 1, buffers, offsets);
        m_boundVertexBuffer = handle;
    }
}

void VulkanRenderer::bindIndexBuffer(BufferHandle handle) {
    auto it = m_buffers.find(handle);
    if (it != m_buffers.end()) {
        vkCmdBindIndexBuffer(m_commandBuffers[m_currentFrame], it->second.buffer, 0, VK_INDEX_TYPE_UINT32);
        m_boundIndexBuffer = handle;
    }
}

void VulkanRenderer::bindTexture(uint32_t slot, TextureHandle handle) {
    m_boundTextures[slot] = handle;
}

void VulkanRenderer::setUniforms(const UniformData& uniforms) {
    void* data;
    vkMapMemory(m_device, m_uniformBuffers[m_currentFrame].memory, 0, sizeof(UniformData), 0, &data);
    memcpy(data, &uniforms, sizeof(UniformData));
    vkUnmapMemory(m_device, m_uniformBuffers[m_currentFrame].memory);
}

void VulkanRenderer::draw(uint32_t vertexCount, uint32_t firstVertex) {
    // Auto-bind default pipeline if none bound
    if (m_boundPipeline == InvalidPipeline && m_defaultPipeline != InvalidPipeline) {
        bindPipeline(m_defaultPipeline);
    }
    vkCmdDraw(m_commandBuffers[m_currentFrame], vertexCount, 1, firstVertex, 0);
    m_stats.drawCalls++;
    m_stats.vertices += vertexCount;
    m_stats.triangles += vertexCount / 3;
}

void VulkanRenderer::drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    // Auto-bind default pipeline if none bound
    if (m_boundPipeline == InvalidPipeline && m_defaultPipeline != InvalidPipeline) {
        bindPipeline(m_defaultPipeline);
    }
    vkCmdDrawIndexed(m_commandBuffers[m_currentFrame], indexCount, 1, firstIndex, vertexOffset, 0);
    m_stats.drawCalls++;
    m_stats.triangles += indexCount / 3;
}

RenderStats VulkanRenderer::getStats() const {
    return m_stats;
}

RenderCaps VulkanRenderer::getCaps() const {
    return m_caps;
}

// Helper implementations

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, uint32_t properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return 0;
}

bool VulkanRenderer::createVkBuffer(size_t size, uint32_t usage, uint32_t properties,
                                    VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        return false;
    }

    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

void VulkanRenderer::copyBuffer(VkBuffer src, VkBuffer dst, size_t size) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

    endSingleTimeCommands(cmd);
}

bool VulkanRenderer::createImage(uint32_t width, uint32_t height, uint32_t format,
                                 uint32_t tiling, uint32_t usage, uint32_t properties,
                                 VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = static_cast<VkFormat>(format);
    imageInfo.tiling = static_cast<VkImageTiling>(tiling);
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        return false;
    }

    vkBindImageMemory(m_device, image, memory, 0);
    return true;
}

VkImageView VulkanRenderer::createImageView(VkImage image, uint32_t format, uint32_t aspectFlags) {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = static_cast<VkFormat>(format);
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view;
    vkCreateImageView(m_device, &viewInfo, nullptr, &view);
    return view;
}

void VulkanRenderer::transitionImageLayout(VkImage image, uint32_t format,
                                           uint32_t oldLayout, uint32_t newLayout) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = static_cast<VkImageLayout>(oldLayout);
    barrier.newLayout = static_cast<VkImageLayout>(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(cmd);
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(cmd);
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

} // namespace opensaints

#else // No Vulkan

#include <iostream>

namespace opensaints {

std::unique_ptr<Renderer> Renderer::create() {
    std::cerr << "Vulkan not available. Build with -DBUILD_RENDERER=ON and install Vulkan SDK.\n";
    return nullptr;
}

} // namespace opensaints

#endif // HAVE_VULKAN
