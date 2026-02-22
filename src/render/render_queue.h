#pragma once
// Render queue for OpenSaints
// Collects draw commands and flushes them sorted by material to minimize state changes

#include "renderer.h"
#include <vector>
#include <algorithm>

namespace opensaints {

// A single queued draw command
struct RenderItem {
    BufferHandle vertexBuffer = InvalidBuffer;
    BufferHandle indexBuffer = InvalidBuffer;
    TextureHandle texture = InvalidTexture;
    uint32_t indexCount = 0;
    uint32_t indexStart = 0;
    int32_t vertexOffset = 0;
    uint32_t vertexCount = 0;     // Used for non-indexed draws
    uint32_t vertexStart = 0;
    float modelMatrix[16];        // Per-object transform
    float sortDepth = 0.0f;       // Distance from camera (for transparency sorting)
};

// Batched render queue that sorts draw calls to minimize state changes
class RenderQueue {
public:
    RenderQueue() = default;

    // Clear all queued items (call at start of frame)
    void clear();

    // Submit a draw item to the queue
    void submit(const RenderItem& item);

    // Flush all queued items to the renderer, sorted by texture
    // Sets per-object model matrix in uniforms before each draw
    void flush(Renderer* renderer, UniformData& uniforms);

    // Stats
    uint32_t itemCount() const { return static_cast<uint32_t>(m_items.size()); }
    uint32_t lastDrawCalls() const { return m_lastDrawCalls; }
    uint32_t lastTextureBinds() const { return m_lastTextureBinds; }

private:
    std::vector<RenderItem> m_items;
    uint32_t m_lastDrawCalls = 0;
    uint32_t m_lastTextureBinds = 0;
};

} // namespace opensaints
