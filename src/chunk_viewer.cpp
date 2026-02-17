// OpenSaints Chunk Viewer
// SDL2+Vulkan viewer for visually validating chunk geometry
// Usage: chunk_viewer <path-to-extracted-chunks>

#define SDL_MAIN_HANDLED
#include "platform/application.h"
#include "render/renderer.h"
#include "formats/chunk.h"
#include "formats/mesh.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <memory>

namespace fs = std::filesystem;

namespace opensaints {

struct LoadedChunk {
    std::string filename;
    WorldChunk chunk;
    std::vector<RenderVertex> vertices;
    std::vector<uint32_t> indices;
    BufferHandle vertexBuffer = InvalidBuffer;
    BufferHandle indexBuffer = InvalidBuffer;
};

class ChunkViewerScene {
public:
    bool initialize(Application* app, Renderer* renderer, const std::string& chunkPath);
    void update(float deltaTime);
    void render();
    void shutdown();

private:
    void scanDirectory(const fs::path& dir);
    bool loadChunk(size_t index);
    void uploadCurrentChunk();
    void destroyGPUResources();
    void nextChunk();
    void prevChunk();
    void printCurrentChunk();

    Application* m_app = nullptr;
    Renderer* m_renderer = nullptr;

    // Chunk data
    fs::path m_chunkDir;
    std::vector<fs::path> m_chunkFiles;
    size_t m_currentIndex = 0;
    LoadedChunk m_current;
    bool m_loaded = false;

    // FPS camera
    float m_camX = 0, m_camY = 50, m_camZ = 0;
    float m_camYaw = 0;
    float m_camPitch = -0.3f;
    float m_moveSpeed = 200.0f;
    float m_lookSpeed = 0.003f;
    bool m_mouseCaptured = false;

    // Input edge detection
    bool m_prevLeftKey = false;
    bool m_prevRightKey = false;
    bool m_prevTab = false;
};

bool ChunkViewerScene::initialize(Application* app, Renderer* renderer, const std::string& chunkPath) {
    m_app = app;
    m_renderer = renderer;
    m_chunkDir = chunkPath;

    std::cout << "\n=== OpenSaints Chunk Viewer ===\n";
    std::cout << "Controls:\n";
    std::cout << "  WASD + Mouse     - FPS camera movement\n";
    std::cout << "  Space/LShift     - Move up/down\n";
    std::cout << "  Left/Right Arrow - Previous/Next chunk\n";
    std::cout << "  Tab              - Toggle mouse capture\n";
    std::cout << "  Q/E              - Decrease/Increase move speed\n";
    std::cout << "  ESC              - Exit\n";
    std::cout << "===============================\n\n";

    scanDirectory(chunkPath);
    std::cout << "Found " << m_chunkFiles.size() << " chunk files\n\n";

    if (!m_chunkFiles.empty()) {
        loadChunk(0);
    }

    return true;
}

void ChunkViewerScene::scanDirectory(const fs::path& dir) {
    if (!fs::exists(dir)) {
        std::cerr << "Directory not found: " << dir << "\n";
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        // Only include .chunk_pc files (not .g_chunk_pc)
        if (ext == ".chunk_pc") {
            std::string filename = entry.path().filename().string();
            if (filename.find(".g_chunk_pc") == std::string::npos &&
                filename.find(".g_peg_pc") == std::string::npos) {
                m_chunkFiles.push_back(entry.path());
            }
        }
    }

    std::sort(m_chunkFiles.begin(), m_chunkFiles.end());
}

bool ChunkViewerScene::loadChunk(size_t index) {
    if (index >= m_chunkFiles.size()) return false;

    destroyGPUResources();
    m_loaded = false;
    m_current = LoadedChunk{};
    m_currentIndex = index;

    const auto& path = m_chunkFiles[index];
    m_current.filename = path.filename().string();

    std::cout << "Loading: " << m_current.filename << "\n";

    if (!m_current.chunk.open(path)) {
        std::cerr << "Failed to load chunk\n";
        return false;
    }

    const auto& data = m_current.chunk.data();

    // Check if there's geometry
    if (data.meshes.empty()) {
        std::cout << "  No geometry (header-only chunk)\n";
        printCurrentChunk();
        // Still consider it "loaded" for browsing
        m_loaded = false;

        // Move camera to chunk center anyway
        m_camX = (data.bounds_min[0] + data.bounds_max[0]) * 0.5f;
        m_camY = data.bounds_max[1] + 50.0f;
        m_camZ = (data.bounds_min[2] + data.bounds_max[2]) * 0.5f;
        return true;
    }

    const auto& mesh = *data.meshes[0];
    if (mesh.submeshes.empty() || mesh.submeshes[0].vertices.empty()) {
        std::cout << "  Mesh has no vertices\n";
        return false;
    }

    // Convert all submeshes to render vertices
    for (const auto& submesh : mesh.submeshes) {
        uint32_t baseVertex = static_cast<uint32_t>(m_current.vertices.size());

        for (const auto& v : submesh.vertices) {
            RenderVertex rv;
            rv.position[0] = v.position.x;
            rv.position[1] = v.position.y;
            rv.position[2] = v.position.z;
            rv.normal[0] = v.normal.x;
            rv.normal[1] = v.normal.y;
            rv.normal[2] = v.normal.z;
            rv.texcoord[0] = v.texcoord0.u;
            rv.texcoord[1] = v.texcoord0.v;

            // Normal-as-color visualization
            uint8_t r = static_cast<uint8_t>((v.normal.x * 0.5f + 0.5f) * 255);
            uint8_t g = static_cast<uint8_t>((v.normal.y * 0.5f + 0.5f) * 255);
            uint8_t b = static_cast<uint8_t>((v.normal.z * 0.5f + 0.5f) * 255);
            rv.color = 0xFF000000 | (b << 16) | (g << 8) | r;

            m_current.vertices.push_back(rv);
        }

        for (uint32_t idx : submesh.indices) {
            m_current.indices.push_back(baseVertex + idx);
        }
    }

    // Upload to GPU
    uploadCurrentChunk();

    // Move camera to chunk center, slightly above
    m_camX = (data.bounds_min[0] + data.bounds_max[0]) * 0.5f;
    m_camY = data.bounds_max[1] + 50.0f;
    m_camZ = (data.bounds_min[2] + data.bounds_max[2]) * 0.5f;

    m_loaded = true;
    printCurrentChunk();
    return true;
}

void ChunkViewerScene::uploadCurrentChunk() {
    if (m_current.vertices.empty()) return;

    m_current.vertexBuffer = m_renderer->createBuffer(
        BufferType::Vertex,
        m_current.vertices.data(),
        m_current.vertices.size() * sizeof(RenderVertex)
    );

    if (!m_current.indices.empty()) {
        m_current.indexBuffer = m_renderer->createBuffer(
            BufferType::Index,
            m_current.indices.data(),
            m_current.indices.size() * sizeof(uint32_t)
        );
    }
}

void ChunkViewerScene::destroyGPUResources() {
    if (m_current.vertexBuffer != InvalidBuffer) {
        m_renderer->destroyBuffer(m_current.vertexBuffer);
        m_current.vertexBuffer = InvalidBuffer;
    }
    if (m_current.indexBuffer != InvalidBuffer) {
        m_renderer->destroyBuffer(m_current.indexBuffer);
        m_current.indexBuffer = InvalidBuffer;
    }
}

void ChunkViewerScene::nextChunk() {
    if (m_chunkFiles.empty()) return;
    loadChunk((m_currentIndex + 1) % m_chunkFiles.size());
}

void ChunkViewerScene::prevChunk() {
    if (m_chunkFiles.empty()) return;
    loadChunk((m_currentIndex + m_chunkFiles.size() - 1) % m_chunkFiles.size());
}

void ChunkViewerScene::printCurrentChunk() {
    const auto& data = m_current.chunk.data();
    std::cout << "  [" << (m_currentIndex + 1) << "/" << m_chunkFiles.size() << "] "
              << m_current.filename << "\n";
    std::cout << "    Bounds: (" << data.bounds_min[0] << ", " << data.bounds_min[1]
              << ", " << data.bounds_min[2] << ") - ("
              << data.bounds_max[0] << ", " << data.bounds_max[1]
              << ", " << data.bounds_max[2] << ")\n";
    std::cout << "    Verts: " << m_current.vertices.size()
              << "  Tris: " << m_current.indices.size() / 3
              << "  Textures: " << data.textures.size() << "\n";
    if (!data.textures.empty()) {
        std::cout << "    First texture: " << data.textures[0] << "\n";
    }
}

void ChunkViewerScene::update(float deltaTime) {
    // ESC to quit
    if (m_app->isKeyDown(Key::Escape)) {
        m_app->quit();
        return;
    }

    // Tab to toggle mouse capture
    bool tabKey = m_app->isKeyDown(Key::Tab);
    if (tabKey && !m_prevTab) {
        m_mouseCaptured = !m_mouseCaptured;
        m_app->captureMouse(m_mouseCaptured);
        std::cout << "Mouse capture: " << (m_mouseCaptured ? "ON" : "OFF") << "\n";
    }
    m_prevTab = tabKey;

    // Chunk navigation
    bool leftKey = m_app->isKeyDown(Key::Left);
    bool rightKey = m_app->isKeyDown(Key::Right);
    if (leftKey && !m_prevLeftKey) prevChunk();
    if (rightKey && !m_prevRightKey) nextChunk();
    m_prevLeftKey = leftKey;
    m_prevRightKey = rightKey;

    // Speed adjustment
    if (m_app->isKeyDown(Key::Q)) m_moveSpeed = std::max(10.0f, m_moveSpeed * 0.95f);
    if (m_app->isKeyDown(Key::E)) m_moveSpeed = std::min(5000.0f, m_moveSpeed * 1.05f);

    // Camera rotation via mouse
    const auto& input = m_app->input();
    if (m_mouseCaptured) {
        m_camYaw -= input.mouse_delta_x * m_lookSpeed;
        m_camPitch -= input.mouse_delta_y * m_lookSpeed;
        m_camPitch = std::max(-1.5f, std::min(1.5f, m_camPitch));
    } else if (input.mouse_buttons[MouseButton::Right]) {
        m_camYaw -= input.mouse_delta_x * m_lookSpeed;
        m_camPitch -= input.mouse_delta_y * m_lookSpeed;
        m_camPitch = std::max(-1.5f, std::min(1.5f, m_camPitch));
    }

    // FPS camera movement
    float forward = 0, right = 0, up = 0;
    if (m_app->isKeyDown(Key::W)) forward += 1;
    if (m_app->isKeyDown(Key::S)) forward -= 1;
    if (m_app->isKeyDown(Key::D)) right += 1;
    if (m_app->isKeyDown(Key::A)) right -= 1;
    if (m_app->isKeyDown(Key::Space)) up += 1;
    if (m_app->isKeyDown(Key::LShift)) up -= 1;

    float speed = m_moveSpeed * deltaTime;
    float cosYaw = std::cos(m_camYaw);
    float sinYaw = std::sin(m_camYaw);

    m_camX += (-sinYaw * forward + cosYaw * right) * speed;
    m_camZ += (cosYaw * forward + sinYaw * right) * speed;
    m_camY += up * speed;

    // Mouse wheel zoom
    if (input.mouse_wheel != 0) {
        float zoomDir = input.mouse_wheel > 0 ? 1.0f : -1.0f;
        m_camX += -sinYaw * std::cos(m_camPitch) * zoomDir * speed * 5.0f;
        m_camY += std::sin(m_camPitch) * zoomDir * speed * 5.0f;
        m_camZ += cosYaw * std::cos(m_camPitch) * zoomDir * speed * 5.0f;
    }
}

void ChunkViewerScene::render() {
    if (!m_loaded || m_current.vertexBuffer == InvalidBuffer) {
        return;
    }

    // Camera forward direction
    float cosPitch = std::cos(m_camPitch);
    float dirX = -std::sin(m_camYaw) * cosPitch;
    float dirY = std::sin(m_camPitch);
    float dirZ = std::cos(m_camYaw) * cosPitch;

    float targetX = m_camX + dirX;
    float targetY = m_camY + dirY;
    float targetZ = m_camZ + dirZ;

    // Setup uniforms
    UniformData uniforms;

    // Model matrix (identity - world space)
    RenderMath::identity(uniforms.modelMatrix);

    // View matrix
    float cameraPos[3] = {m_camX, m_camY, m_camZ};
    float target[3] = {targetX, targetY, targetZ};
    float up[3] = {0, 1, 0};
    RenderMath::lookAt(uniforms.viewMatrix, cameraPos, target, up);

    // Projection matrix
    float aspect = m_app->aspectRatio();
    RenderMath::perspective(uniforms.projectionMatrix, 3.14159f / 4.0f, aspect, 1.0f, 10000.0f);

    // Camera and lighting
    uniforms.cameraPosition[0] = m_camX;
    uniforms.cameraPosition[1] = m_camY;
    uniforms.cameraPosition[2] = m_camZ;
    uniforms.cameraPosition[3] = 1.0f;

    uniforms.lightDirection[0] = -0.3f;
    uniforms.lightDirection[1] = -1.0f;
    uniforms.lightDirection[2] = -0.5f;
    uniforms.lightDirection[3] = 0.0f;

    uniforms.lightColor[0] = 1.0f;
    uniforms.lightColor[1] = 1.0f;
    uniforms.lightColor[2] = 0.9f;
    uniforms.lightColor[3] = 1.0f;

    uniforms.ambientColor[0] = 0.4f;
    uniforms.ambientColor[1] = 0.4f;
    uniforms.ambientColor[2] = 0.45f;
    uniforms.ambientColor[3] = 1.0f;

    m_renderer->setUniforms(uniforms);

    // Draw chunk
    m_renderer->bindVertexBuffer(m_current.vertexBuffer);
    if (m_current.indexBuffer != InvalidBuffer && !m_current.indices.empty()) {
        m_renderer->bindIndexBuffer(m_current.indexBuffer);
        m_renderer->drawIndexed(static_cast<uint32_t>(m_current.indices.size()));
    } else {
        m_renderer->draw(static_cast<uint32_t>(m_current.vertices.size()));
    }
}

void ChunkViewerScene::shutdown() {
    destroyGPUResources();
}

} // namespace opensaints

void printUsage(const char* prog) {
    std::cout << "OpenSaints Chunk Viewer\n\n";
    std::cout << "Usage: " << prog << " <path-to-extracted-chunks>\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << prog << " D:/OpenSaints/temp_chunk_extract\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    using namespace opensaints;

    std::cout << "OpenSaints Chunk Viewer\n\n";

    // Create application
    Application app;
    WindowConfig config;
    config.title = "OpenSaints Chunk Viewer";
    config.width = 1280;
    config.height = 720;

    if (!app.initialize(config)) {
        std::cerr << "Failed to initialize application\n";
        return 1;
    }

    // Create renderer
    auto renderer = Renderer::create();
    if (!renderer || !renderer->initialize(&app)) {
        std::cerr << "Failed to initialize renderer\n";
        return 1;
    }

    // Create chunk viewer scene
    ChunkViewerScene viewer;
    if (!viewer.initialize(&app, renderer.get(), argv[1])) {
        std::cerr << "Failed to initialize chunk viewer\n";
        return 1;
    }

    // Set callbacks
    app.setUpdateCallback([&viewer](float dt) {
        viewer.update(dt);
    });

    app.setRenderCallback([&viewer, &renderer]() {
        if (renderer->beginFrame()) {
            renderer->clear(Color(0.15f, 0.15f, 0.2f, 1.0f));
            viewer.render();
            renderer->endFrame();
        }
    });

    app.setResizeCallback([&renderer](int w, int h) {
        renderer->onResize(w, h);
    });

    // Run main loop
    app.run();

    // Cleanup
    viewer.shutdown();
    renderer->shutdown();
    app.shutdown();

    return 0;
}
