#include "render_queue.h"
#include <cstring>

namespace opensaints {

void RenderQueue::clear() {
    m_items.clear();
}

void RenderQueue::submit(const RenderItem& item) {
    m_items.push_back(item);
}

void RenderQueue::flush(Renderer* renderer, UniformData& uniforms) {
    if (m_items.empty()) {
        m_lastDrawCalls = 0;
        m_lastTextureBinds = 0;
        return;
    }

    // Sort by: vertex buffer -> texture -> index buffer
    // This groups draws that share the same GPU state together,
    // minimizing expensive state changes (especially texture binds)
    std::sort(m_items.begin(), m_items.end(),
        [](const RenderItem& a, const RenderItem& b) {
            if (a.vertexBuffer != b.vertexBuffer) return a.vertexBuffer < b.vertexBuffer;
            if (a.texture != b.texture) return a.texture < b.texture;
            return a.indexBuffer < b.indexBuffer;
        });

    uint32_t drawCalls = 0;
    uint32_t textureBinds = 0;

    BufferHandle boundVB = InvalidBuffer;
    BufferHandle boundIB = InvalidBuffer;
    TextureHandle boundTex = InvalidTexture;

    for (const auto& item : m_items) {
        // Bind vertex buffer if changed
        if (item.vertexBuffer != boundVB) {
            renderer->bindVertexBuffer(item.vertexBuffer);
            boundVB = item.vertexBuffer;
        }

        // Bind index buffer if changed
        if (item.indexBuffer != boundIB && item.indexBuffer != InvalidBuffer) {
            renderer->bindIndexBuffer(item.indexBuffer);
            boundIB = item.indexBuffer;
        }

        // Bind texture if changed
        if (item.texture != boundTex) {
            renderer->bindTexture(0, item.texture);
            boundTex = item.texture;
            textureBinds++;
        }

        // Set per-object model matrix
        std::memcpy(uniforms.modelMatrix, item.modelMatrix, sizeof(float) * 16);
        renderer->setUniforms(uniforms);

        // Draw
        if (item.indexBuffer != InvalidBuffer && item.indexCount > 0) {
            renderer->drawIndexed(item.indexCount, item.indexStart, item.vertexOffset);
        } else if (item.vertexCount > 0) {
            renderer->draw(item.vertexCount, item.vertexStart);
        }

        drawCalls++;
    }

    m_lastDrawCalls = drawCalls;
    m_lastTextureBinds = textureBinds;
}

} // namespace opensaints
