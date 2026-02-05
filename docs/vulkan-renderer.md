# Vulkan Renderer Implementation Notes

This document covers the implementation details, issues encountered, and solutions found while building the Vulkan rendering backend for OpenSaints.

## Overview

The Vulkan renderer (`src/render/vulkan_backend.cpp`) provides a modern GPU rendering backend using the Vulkan API with dynamic function loading via volk.

### Dependencies

- **volk**: Dynamic Vulkan function loader (no SDK dependency at runtime)
- **SDL2**: Window creation and Vulkan surface creation via `SDL_Vulkan_CreateSurface`
- **Vulkan Headers**: Type definitions and constants

### Key Files

| File | Purpose |
|------|---------|
| `src/render/vulkan_backend.h` | Renderer class declaration |
| `src/render/vulkan_backend.cpp` | Full implementation |
| `src/render/default_shaders.h` | Embedded SPIR-V bytecode |
| `src/render/shaders/basic.vert` | Vertex shader source (GLSL 450) |
| `src/render/shaders/basic.frag` | Fragment shader source (GLSL 450) |

## Architecture

### Initialization Order

1. `volkInitialize()` - Load Vulkan library dynamically
2. `createInstance()` - Create VkInstance
3. `volkLoadInstance()` - Load instance-level functions
4. `createSurface()` - Create window surface via SDL2
5. `selectPhysicalDevice()` - Choose GPU
6. `createLogicalDevice()` - Create VkDevice with queues
7. `volkLoadDevice()` - Load device-level functions
8. `createSwapchain()` - Create swapchain matching surface format
9. `createRenderPass()` - Create render pass (MUST use actual swapchain format)
10. `createFramebuffers()` - Create framebuffers with depth attachment
11. `createCommandPool()` / `createCommandBuffers()` - Command recording
12. `createSyncObjects()` - Semaphores and fences for frame pacing
13. `createDescriptorSetLayout()` / `createDescriptorPool()` - Descriptor management
14. `createUniformBuffers()` - Per-frame uniform buffers + descriptor sets
15. `createDefaultResources()` - Sampler, white texture, default pipeline

### Per-Frame Rendering

```
beginFrame()
  ├─ vkWaitForFences (wait for previous frame using this slot)
  ├─ vkAcquireNextImageKHR (get swapchain image)
  ├─ vkResetCommandBuffer
  ├─ vkBeginCommandBuffer
  ├─ vkCmdBeginRenderPass (with clear values)
  ├─ vkCmdSetViewport / vkCmdSetScissor
  └─ Reset bound state tracking (CRITICAL - see issues below)

[Application draw calls]
  ├─ setUniforms() - Map and update uniform buffer
  ├─ bindPipeline() - Bind pipeline + descriptor sets
  ├─ bindVertexBuffer() / bindIndexBuffer()
  └─ draw() / drawIndexed()

endFrame()
  ├─ vkCmdEndRenderPass
  ├─ vkEndCommandBuffer
  ├─ vkQueueSubmit (with semaphore synchronization)
  └─ vkQueuePresentKHR
```

## Issues Encountered and Solutions

### Issue 1: SDL_main Linker Error (Windows)

**Symptom:** `unresolved external symbol SDL_main`

**Cause:** SDL2 on Windows redefines `main` to `SDL_main` and provides its own entry point.

**Solution:** Add before including SDL headers:
```cpp
#define SDL_MAIN_HANDLED
```
Then call `SDL_SetMainReady()` if needed, or just define a standard `main()`.

### Issue 2: Black Screen (Swapchain Format Mismatch)

**Symptom:** Window opens, clears to black, but clear color not visible.

**Cause:** Render pass attachment was hardcoded to `VK_FORMAT_B8G8R8A8_SRGB` but the swapchain might select a different format (e.g., `VK_FORMAT_B8G8R8A8_UNORM`).

**Solution:** Store the actual swapchain format and use it in render pass creation:
```cpp
// In createSwapchain():
m_swapchainFormat = surfaceFormat.format;

// In createRenderPass():
colorAttachment.format = static_cast<VkFormat>(m_swapchainFormat);
```

### Issue 3: VkClearValue Union Initialization

**Symptom:** Clear color not applied, still black screen.

**Cause:** Brace initialization of `VkClearValue` union doesn't work reliably across compilers:
```cpp
// BROKEN:
VkClearValue clearValues[2] = {
    {{{r, g, b, a}}},  // May not initialize color.float32 correctly
    ...
};
```

**Solution:** Explicitly set the union members:
```cpp
VkClearValue clearValues[2] = {};
clearValues[0].color.float32[0] = m_clearColor.r;
clearValues[0].color.float32[1] = m_clearColor.g;
clearValues[0].color.float32[2] = m_clearColor.b;
clearValues[0].color.float32[3] = m_clearColor.a;
clearValues[1].depthStencil.depth = 1.0f;
clearValues[1].depthStencil.stencil = 0;
```

### Issue 4: No Geometry Rendered (Descriptor Sets Not Bound)

**Symptom:** Clear color works, but no geometry visible.

**Cause:** Descriptor sets were created but never allocated from the pool, and never bound to the command buffer.

**Solution:** In `createUniformBuffers()`:
1. Allocate descriptor sets from the pool
2. Update descriptor sets with buffer info
3. In `bindPipeline()`, call `vkCmdBindDescriptorSets()`

```cpp
// Allocate
VkDescriptorSetAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
allocInfo.descriptorPool = m_descriptorPool;
allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
allocInfo.pSetLayouts = layouts.data();
vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data());

// Update with buffer binding
VkWriteDescriptorSet descriptorWrite = {};
descriptorWrite.dstSet = m_descriptorSets[i];
descriptorWrite.dstBinding = 0;
descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
descriptorWrite.pBufferInfo = &bufferInfo;
vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);

// Bind when using pipeline
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, 0, 1, &m_descriptorSets[frame], 0, nullptr);
```

### Issue 5: Triangle Backface Culled

**Symptom:** Cube renders, but triangle invisible.

**Cause:** Triangle vertices were wound clockwise when viewed from the camera, causing the front face to point away. With `VK_CULL_MODE_BACK_BIT`, the visible side was culled.

**Solution:** Ensure vertices are wound counter-clockwise when viewed from the camera (Vulkan default front face is CCW):
```cpp
// Camera at Z=3, looking at origin
// Vertices should go counter-clockwise from camera's view:
{{-0.5f, -0.5f, 0.0f}, ...}, // bottom-left
{{ 0.0f,  0.5f, 0.0f}, ...}, // top (CCW next)
{{ 0.5f, -0.5f, 0.0f}, ...}, // bottom-right
```

### Issue 6: Geometry Disappears After Frame 1 (CRITICAL)

**Symptom:** Geometry visible on first frame, then disappears. Or geometry only partially renders.

**Cause:** The `m_boundPipeline` tracking variable was not reset in `beginFrame()`. After frame 1:
1. `beginFrame()` resets the command buffer (unbinds everything on GPU)
2. But `m_boundPipeline` still holds the old handle
3. `draw()` checks `if (m_boundPipeline == InvalidPipeline)` - this is FALSE
4. Auto-bind is skipped, no pipeline is actually bound
5. Draw call executes with no pipeline = undefined behavior

**Solution:** Reset all bound state tracking at the start of each frame:
```cpp
// In beginFrame(), after command buffer operations:
m_boundPipeline = InvalidPipeline;
m_boundVertexBuffer = InvalidBuffer;
m_boundIndexBuffer = InvalidBuffer;
```

### Issue 7: Multiple Objects Share Same Transform (Uniform Buffer Race)

**Symptom:** Multiple objects drawn with different transforms all use the last transform.

**Cause:** Single uniform buffer per frame, updated by `setUniforms()`. GPU execution is deferred:
1. Triangle: setUniforms(transformA), draw()
2. Cube: setUniforms(transformB), draw()  // Overwrites buffer!
3. GPU executes both draws - both see transformB

**Solution (Workaround):** For the demo, bake vertex positions into the mesh rather than using per-object transforms. Proper solutions for the future:
- Push constants for per-object data
- Dynamic uniform buffers with offsets
- Multiple descriptor sets

## Shader Setup

### Vertex Shader (basic.vert)

```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 lightDir;
    vec4 lightColor;
    vec4 ambientColor;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragNormal = mat3(ubo.model) * inNormal;
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}
```

### Fragment Shader (basic.frag)

```glsl
#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
```

### Compiling Shaders

Requires `glslc` from the Vulkan SDK:
```bash
glslc basic.vert -o basic.vert.spv
glslc basic.frag -o basic.frag.spv
```

Then embed in C++ header using `xxd -i` or similar tool.

## Vertex Format

```cpp
struct RenderVertex {
    float position[3];  // location 0
    float normal[3];    // location 1
    float texcoord[2];  // location 2
    uint32_t color;     // location 3 (RGBA packed)
};
```

Pipeline vertex input configuration must match this layout exactly.

## Future Improvements

1. **Per-object transforms**: Implement push constants or dynamic UBOs
2. **Texture support**: Bind actual textures instead of white default
3. **Lighting**: Implement basic lighting in fragment shader
4. **Validation layers**: Enable in debug builds for error checking
5. **Swapchain recreation**: Handle window resize properly
6. **Multi-threaded command recording**: For complex scenes

## Performance Considerations

- Current implementation uses `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` for uniform buffers (CPU-mappable, slower GPU access)
- Vertex/index buffers use device-local memory with staging (optimal)
- Single draw call per object - no batching yet
- No frustum culling or LOD

## References

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [volk GitHub](https://github.com/zeux/volk)
- [Vulkan Specification](https://www.khronos.org/registry/vulkan/specs/1.3/html/)
