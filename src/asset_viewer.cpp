// OpenSaints Asset Viewer
// Browse and preview Saints Row 2 game assets

#define SDL_MAIN_HANDLED
#include "platform/application.h"
#include "render/renderer.h"
#include "formats/vpp.h"
#include "formats/peg.h"
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

// Asset types we can browse
enum class AssetType {
    Unknown,
    VPP,
    Texture,
    Mesh
};

AssetType getAssetType(const std::string& ext) {
    std::string lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == ".vpp_pc") return AssetType::VPP;
    if (lower == ".peg_pc" || lower == ".cpeg_pc" || lower == ".cvbm_pc") return AssetType::Texture;
    if (lower == ".cmesh_pc" || lower == ".smesh_pc") return AssetType::Mesh;
    return AssetType::Unknown;
}

class AssetViewerScene {
public:
    bool initialize(Application* app, Renderer* renderer, const std::string& assetPath);
    void update(float deltaTime);
    void render();
    void shutdown();

private:
    void scanDirectory(const fs::path& dir);
    void loadMesh(const fs::path& path);
    void nextMesh();
    void prevMesh();
    void printCurrentAsset();

    Application* m_app = nullptr;
    Renderer* m_renderer = nullptr;

    // Asset data
    fs::path m_assetPath;
    std::vector<fs::path> m_meshFiles;
    size_t m_currentMeshIndex = 0;

    // Current mesh (try both types)
    CharacterMesh m_charMesh;
    StaticMesh m_staticMesh;
    MeshData const* m_currentMeshData = nullptr;
    std::vector<RenderVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    bool m_meshLoaded = false;

    // GPU resources
    BufferHandle m_vertexBuffer = InvalidBuffer;
    BufferHandle m_indexBuffer = InvalidBuffer;

    // Camera
    float m_cameraDistance = 2.0f;
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.3f;
    float m_rotation = 0.0f;
    bool m_autoRotate = true;
    bool m_mouseDown = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;

    // Input state
    bool m_prevLeftKey = false;
    bool m_prevRightKey = false;
};

bool AssetViewerScene::initialize(Application* app, Renderer* renderer, const std::string& assetPath) {
    m_app = app;
    m_renderer = renderer;
    m_assetPath = assetPath;

    std::cout << "\n=== OpenSaints Asset Viewer ===\n";
    std::cout << "Controls:\n";
    std::cout << "  Left/Right Arrow - Previous/Next mesh\n";
    std::cout << "  Mouse drag       - Rotate camera\n";
    std::cout << "  Mouse wheel      - Zoom in/out\n";
    std::cout << "  R                - Toggle auto-rotate\n";
    std::cout << "  ESC              - Exit\n";
    std::cout << "===============================\n\n";

    std::cout << "Scanning for assets in: " << assetPath << "\n";
    scanDirectory(assetPath);

    std::cout << "Found " << m_meshFiles.size() << " mesh files\n\n";

    if (!m_meshFiles.empty()) {
        loadMesh(m_meshFiles[0]);
    }

    return true;
}

void AssetViewerScene::scanDirectory(const fs::path& dir) {
    if (!fs::exists(dir)) {
        std::cerr << "Directory not found: " << dir << "\n";
        return;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            if (getAssetType(ext) == AssetType::Mesh) {
                m_meshFiles.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << "\n";
    }

    std::sort(m_meshFiles.begin(), m_meshFiles.end());
}

void AssetViewerScene::loadMesh(const fs::path& path) {
    std::cout << "Loading: " << path.filename() << "\n";

    m_charMesh.close();
    m_staticMesh.close();
    m_currentMeshData = nullptr;
    m_meshLoaded = false;
    m_vertices.clear();
    m_indices.clear();

    // Try to determine mesh type from extension
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool loaded = false;
    if (ext == ".cmesh_pc") {
        loaded = m_charMesh.open(path);
        if (loaded) m_currentMeshData = &m_charMesh.data();
    } else if (ext == ".smesh_pc") {
        loaded = m_staticMesh.open(path);
        if (loaded) m_currentMeshData = &m_staticMesh.data();
    } else {
        // Try character mesh first, then static
        if (m_charMesh.open(path)) {
            loaded = true;
            m_currentMeshData = &m_charMesh.data();
        } else if (m_staticMesh.open(path)) {
            loaded = true;
            m_currentMeshData = &m_staticMesh.data();
        }
    }

    if (!loaded || !m_currentMeshData) {
        std::cerr << "Failed to load mesh\n";
        return;
    }

    const auto& data = *m_currentMeshData;
    if (data.submeshes.empty() || data.submeshes[0].vertices.empty()) {
        std::cerr << "Mesh has no vertices\n";
        return;
    }

    const auto& submesh = data.submeshes[0];

    // Calculate bounding box
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

    for (const auto& v : submesh.vertices) {
        minX = std::min(minX, v.position.x);
        minY = std::min(minY, v.position.y);
        minZ = std::min(minZ, v.position.z);
        maxX = std::max(maxX, v.position.x);
        maxY = std::max(maxY, v.position.y);
        maxZ = std::max(maxZ, v.position.z);
    }

    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    float centerZ = (minZ + maxZ) / 2.0f;
    float size = std::max({maxX - minX, maxY - minY, maxZ - minZ});
    float scale = (size > 0.001f) ? 1.0f / size : 1.0f;

    // Convert vertices
    m_vertices.reserve(submesh.vertices.size());
    for (const auto& v : submesh.vertices) {
        RenderVertex rv;
        rv.position[0] = (v.position.x - centerX) * scale;
        rv.position[1] = (v.position.y - centerY) * scale;
        rv.position[2] = (v.position.z - centerZ) * scale;
        rv.normal[0] = v.normal.x;
        rv.normal[1] = v.normal.y;
        rv.normal[2] = v.normal.z;
        rv.texcoord[0] = v.texcoord0.u;
        rv.texcoord[1] = v.texcoord0.v;

        // Color based on normal for visualization
        uint8_t r = static_cast<uint8_t>((v.normal.x * 0.5f + 0.5f) * 255);
        uint8_t g = static_cast<uint8_t>((v.normal.y * 0.5f + 0.5f) * 255);
        uint8_t b = static_cast<uint8_t>((v.normal.z * 0.5f + 0.5f) * 255);
        rv.color = 0xFF000000 | (b << 16) | (g << 8) | r;

        m_vertices.push_back(rv);
    }

    m_indices = submesh.indices;

    // Destroy old buffers
    if (m_vertexBuffer != InvalidBuffer) {
        m_renderer->destroyBuffer(m_vertexBuffer);
        m_vertexBuffer = InvalidBuffer;
    }
    if (m_indexBuffer != InvalidBuffer) {
        m_renderer->destroyBuffer(m_indexBuffer);
        m_indexBuffer = InvalidBuffer;
    }

    // Create new buffers
    m_vertexBuffer = m_renderer->createBuffer(
        BufferType::Vertex,
        m_vertices.data(),
        m_vertices.size() * sizeof(RenderVertex)
    );

    m_indexBuffer = m_renderer->createBuffer(
        BufferType::Index,
        m_indices.data(),
        m_indices.size() * sizeof(uint32_t)
    );

    m_meshLoaded = true;
    m_cameraDistance = 2.0f;

    printCurrentAsset();
}

void AssetViewerScene::nextMesh() {
    if (m_meshFiles.empty()) return;
    m_currentMeshIndex = (m_currentMeshIndex + 1) % m_meshFiles.size();
    loadMesh(m_meshFiles[m_currentMeshIndex]);
}

void AssetViewerScene::prevMesh() {
    if (m_meshFiles.empty()) return;
    m_currentMeshIndex = (m_currentMeshIndex + m_meshFiles.size() - 1) % m_meshFiles.size();
    loadMesh(m_meshFiles[m_currentMeshIndex]);
}

void AssetViewerScene::printCurrentAsset() {
    if (!m_meshLoaded || !m_currentMeshData) return;

    const auto& data = *m_currentMeshData;
    std::cout << "  [" << (m_currentMeshIndex + 1) << "/" << m_meshFiles.size() << "] "
              << data.name << ": "
              << m_vertices.size() << " verts, "
              << m_indices.size() / 3 << " tris";
    if (!data.materials.empty() && !data.materials[0].diffuse_texture.empty()) {
        std::cout << " | tex: " << data.materials[0].diffuse_texture;
    }
    std::cout << "\n";
}

void AssetViewerScene::update(float deltaTime) {
    // Auto rotate
    if (m_autoRotate) {
        m_rotation += deltaTime * 0.5f;
    }

    // Keyboard navigation (with edge detection)
    bool leftKey = m_app->isKeyDown(Key::Left);
    bool rightKey = m_app->isKeyDown(Key::Right);

    if (leftKey && !m_prevLeftKey) {
        prevMesh();
    }
    if (rightKey && !m_prevRightKey) {
        nextMesh();
    }

    m_prevLeftKey = leftKey;
    m_prevRightKey = rightKey;

    // Toggle auto-rotate
    static bool prevR = false;
    bool rKey = m_app->isKeyDown(Key::R);
    if (rKey && !prevR) {
        m_autoRotate = !m_autoRotate;
        std::cout << "Auto-rotate: " << (m_autoRotate ? "ON" : "OFF") << "\n";
    }
    prevR = rKey;

    // ESC to quit
    if (m_app->isKeyDown(Key::Escape)) {
        m_app->quit();
    }

    // Mouse camera control
    const auto& input = m_app->input();

    if (input.mouse_buttons[MouseButton::Left]) {
        if (!m_mouseDown) {
            m_mouseDown = true;
            m_lastMouseX = input.mouse_x;
            m_lastMouseY = input.mouse_y;
        } else {
            int dx = input.mouse_x - m_lastMouseX;
            int dy = input.mouse_y - m_lastMouseY;
            m_lastMouseX = input.mouse_x;
            m_lastMouseY = input.mouse_y;

            m_cameraYaw += dx * 0.01f;
            m_cameraPitch += dy * 0.01f;
            m_cameraPitch = std::max(-1.5f, std::min(1.5f, m_cameraPitch));
        }
    } else {
        m_mouseDown = false;
    }

    // Mouse wheel zoom
    if (input.mouse_wheel != 0) {
        m_cameraDistance -= input.mouse_wheel * 0.2f;
        m_cameraDistance = std::max(0.5f, std::min(10.0f, m_cameraDistance));
    }
}

void AssetViewerScene::render() {
    if (!m_meshLoaded || m_vertexBuffer == InvalidBuffer) {
        return;
    }

    // Calculate camera position
    float camX = m_cameraDistance * std::cos(m_cameraPitch) * std::sin(m_cameraYaw);
    float camY = m_cameraDistance * std::sin(m_cameraPitch);
    float camZ = m_cameraDistance * std::cos(m_cameraPitch) * std::cos(m_cameraYaw);

    // Setup uniforms
    UniformData uniforms;

    // Model matrix (rotation)
    RenderMath::rotateY(uniforms.modelMatrix, m_rotation);

    // View matrix
    float cameraPos[3] = {camX, camY, camZ};
    float target[3] = {0, 0, 0};
    float up[3] = {0, 1, 0};
    RenderMath::lookAt(uniforms.viewMatrix, cameraPos, target, up);

    // Projection matrix
    float aspect = m_app->aspectRatio();
    RenderMath::perspective(uniforms.projectionMatrix, 3.14159f / 4.0f, aspect, 0.1f, 100.0f);

    // Camera and lighting
    uniforms.cameraPosition[0] = camX;
    uniforms.cameraPosition[1] = camY;
    uniforms.cameraPosition[2] = camZ;
    uniforms.cameraPosition[3] = 1.0f;

    uniforms.lightDirection[0] = -0.5f;
    uniforms.lightDirection[1] = -1.0f;
    uniforms.lightDirection[2] = -0.5f;
    uniforms.lightDirection[3] = 0.0f;

    uniforms.lightColor[0] = 1.0f;
    uniforms.lightColor[1] = 1.0f;
    uniforms.lightColor[2] = 0.9f;
    uniforms.lightColor[3] = 1.0f;

    uniforms.ambientColor[0] = 0.3f;
    uniforms.ambientColor[1] = 0.3f;
    uniforms.ambientColor[2] = 0.35f;
    uniforms.ambientColor[3] = 1.0f;

    m_renderer->setUniforms(uniforms);

    // Draw mesh
    m_renderer->bindVertexBuffer(m_vertexBuffer);

    // Try indexed draw first, fall back to direct vertex draw
    if (m_indexBuffer != InvalidBuffer && !m_indices.empty()) {
        m_renderer->bindIndexBuffer(m_indexBuffer);
        m_renderer->drawIndexed(static_cast<uint32_t>(m_indices.size()));
    } else {
        // Draw as triangle list directly
        m_renderer->draw(static_cast<uint32_t>(m_vertices.size()));
    }
}

void AssetViewerScene::shutdown() {
    if (m_vertexBuffer != InvalidBuffer) {
        m_renderer->destroyBuffer(m_vertexBuffer);
    }
    if (m_indexBuffer != InvalidBuffer) {
        m_renderer->destroyBuffer(m_indexBuffer);
    }
}

} // namespace opensaints

void printUsage(const char* prog) {
    std::cout << "OpenSaints Asset Viewer\n\n";
    std::cout << "Usage: " << prog << " <path-to-extracted-meshes>\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << prog << " D:/OpenSaints/test_extract/meshes\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    using namespace opensaints;

    std::cout << "OpenSaints Asset Viewer\n";
    std::cout << "Press Left/Right arrows to browse meshes\n\n";

    // Create application
    Application app;
    WindowConfig config;
    config.title = "OpenSaints Asset Viewer";
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

    // Create asset viewer scene
    AssetViewerScene viewer;
    if (!viewer.initialize(&app, renderer.get(), argv[1])) {
        std::cerr << "Failed to initialize asset viewer\n";
        return 1;
    }

    // Set callbacks
    app.setUpdateCallback([&viewer](float dt) {
        viewer.update(dt);
    });

    app.setRenderCallback([&viewer, &renderer]() {
        if (renderer->beginFrame()) {
            renderer->clear(Color(0.2f, 0.2f, 0.25f, 1.0f));
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
