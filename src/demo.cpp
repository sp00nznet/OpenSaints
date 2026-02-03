// OpenSaints Demo/Test Scene
// Simple scene to verify rendering pipeline works

#include "platform/application.h"
#include "render/renderer.h"
#include "engine/vfs.h"
#include "engine/asset_manager.h"
#include <iostream>
#include <cmath>

namespace opensaints {

class DemoScene {
public:
    DemoScene() = default;
    ~DemoScene() = default;

    bool initialize(Application* app, Renderer* renderer) {
        m_app = app;
        m_renderer = renderer;

        // Create a simple triangle mesh
        RenderVertex vertices[] = {
            // Position           Normal            UV        Color
            {{-0.5f, -0.5f, 0.0f}, {0, 0, 1}, {0, 1}, 0xFFFF0000}, // Red
            {{ 0.5f, -0.5f, 0.0f}, {0, 0, 1}, {1, 1}, 0xFF00FF00}, // Green
            {{ 0.0f,  0.5f, 0.0f}, {0, 0, 1}, {0.5f, 0}, 0xFF0000FF}, // Blue
        };

        m_triangleBuffer = renderer->createBuffer(
            BufferType::Vertex,
            vertices,
            sizeof(vertices)
        );

        // Create a simple cube mesh
        createCubeMesh();

        // Initialize camera
        m_cameraPos[0] = 0;
        m_cameraPos[1] = 0;
        m_cameraPos[2] = 3;

        m_cameraTarget[0] = 0;
        m_cameraTarget[1] = 0;
        m_cameraTarget[2] = 0;

        return true;
    }

    void update(float deltaTime) {
        // Rotate object
        m_rotation += deltaTime * 0.5f;

        // Handle input
        if (m_app->isKeyDown(Key::W)) {
            m_cameraPos[2] -= deltaTime * 2.0f;
        }
        if (m_app->isKeyDown(Key::S)) {
            m_cameraPos[2] += deltaTime * 2.0f;
        }
        if (m_app->isKeyDown(Key::A)) {
            m_cameraPos[0] -= deltaTime * 2.0f;
        }
        if (m_app->isKeyDown(Key::D)) {
            m_cameraPos[0] += deltaTime * 2.0f;
        }

        // Mouse look (when captured)
        if (m_app->isMouseCaptured()) {
            m_yaw += m_app->input().mouse_delta_x * 0.002f;
            m_pitch -= m_app->input().mouse_delta_y * 0.002f;

            // Clamp pitch
            if (m_pitch > 1.5f) m_pitch = 1.5f;
            if (m_pitch < -1.5f) m_pitch = -1.5f;

            // Update camera target based on yaw/pitch
            m_cameraTarget[0] = m_cameraPos[0] + std::cos(m_pitch) * std::sin(m_yaw);
            m_cameraTarget[1] = m_cameraPos[1] + std::sin(m_pitch);
            m_cameraTarget[2] = m_cameraPos[2] - std::cos(m_pitch) * std::cos(m_yaw);
        }

        // Toggle mouse capture with left click
        if (m_app->input().mouse_buttons[MouseButton::Left] && !m_app->isMouseCaptured()) {
            m_app->captureMouse(true);
        }
    }

    void render() {
        // Setup uniforms
        UniformData uniforms;

        // Model matrix (rotate around Y)
        RenderMath::rotateY(uniforms.modelMatrix, m_rotation);

        // View matrix (camera)
        float up[3] = {0, 1, 0};
        RenderMath::lookAt(uniforms.viewMatrix, m_cameraPos, m_cameraTarget, up);

        // Projection matrix
        float aspect = m_app->aspectRatio();
        RenderMath::perspective(uniforms.projectionMatrix, 3.14159f / 4.0f, aspect, 0.1f, 100.0f);

        // Camera and lighting
        uniforms.cameraPosition[0] = m_cameraPos[0];
        uniforms.cameraPosition[1] = m_cameraPos[1];
        uniforms.cameraPosition[2] = m_cameraPos[2];
        uniforms.cameraPosition[3] = 1.0f;

        uniforms.lightDirection[0] = -0.5f;
        uniforms.lightDirection[1] = -1.0f;
        uniforms.lightDirection[2] = -0.5f;
        uniforms.lightDirection[3] = 0.0f;

        uniforms.lightColor[0] = 1.0f;
        uniforms.lightColor[1] = 1.0f;
        uniforms.lightColor[2] = 0.9f;
        uniforms.lightColor[3] = 1.0f;

        uniforms.ambientColor[0] = 0.2f;
        uniforms.ambientColor[1] = 0.2f;
        uniforms.ambientColor[2] = 0.3f;
        uniforms.ambientColor[3] = 1.0f;

        m_renderer->setUniforms(uniforms);

        // Draw triangle
        if (m_triangleBuffer != InvalidBuffer) {
            m_renderer->bindVertexBuffer(m_triangleBuffer);
            m_renderer->draw(3);
        }

        // Draw cube with offset
        if (m_cubeVertexBuffer != InvalidBuffer && m_cubeIndexBuffer != InvalidBuffer) {
            // Offset the cube
            float cubeModel[16];
            RenderMath::translate(cubeModel, 2.0f, 0, 0);
            float rotated[16];
            RenderMath::rotateY(rotated, m_rotation * 0.7f);
            RenderMath::multiply(uniforms.modelMatrix, cubeModel, rotated);
            m_renderer->setUniforms(uniforms);

            m_renderer->bindVertexBuffer(m_cubeVertexBuffer);
            m_renderer->bindIndexBuffer(m_cubeIndexBuffer);
            m_renderer->drawIndexed(36);
        }
    }

    void shutdown() {
        if (m_triangleBuffer != InvalidBuffer) {
            m_renderer->destroyBuffer(m_triangleBuffer);
        }
        if (m_cubeVertexBuffer != InvalidBuffer) {
            m_renderer->destroyBuffer(m_cubeVertexBuffer);
        }
        if (m_cubeIndexBuffer != InvalidBuffer) {
            m_renderer->destroyBuffer(m_cubeIndexBuffer);
        }
    }

private:
    Application* m_app = nullptr;
    Renderer* m_renderer = nullptr;

    BufferHandle m_triangleBuffer = InvalidBuffer;
    BufferHandle m_cubeVertexBuffer = InvalidBuffer;
    BufferHandle m_cubeIndexBuffer = InvalidBuffer;

    float m_rotation = 0;
    float m_cameraPos[3] = {0, 0, 3};
    float m_cameraTarget[3] = {0, 0, 0};
    float m_yaw = 0;
    float m_pitch = 0;

    void createCubeMesh() {
        // Cube vertices with normals
        RenderVertex vertices[] = {
            // Front face (Z+)
            {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 1}, 0xFFFFFFFF},
            {{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 1}, 0xFFFFFFFF},
            {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 0}, 0xFFFFFFFF},
            {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 0}, 0xFFFFFFFF},

            // Back face (Z-)
            {{ 0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}, 0xFFFFFFFF},
            {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}, 0xFFFFFFFF},
            {{-0.5f,  0.5f, -0.5f}, {0, 0, -1}, {1, 0}, 0xFFFFFFFF},
            {{ 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 0}, 0xFFFFFFFF},

            // Top face (Y+)
            {{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 1}, 0xFFFFFFFF},
            {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 1}, 0xFFFFFFFF},
            {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 0}, 0xFFFFFFFF},
            {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 0}, 0xFFFFFFFF},

            // Bottom face (Y-)
            {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}, 0xFFFFFFFF},
            {{ 0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 1}, 0xFFFFFFFF},
            {{ 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 0}, 0xFFFFFFFF},
            {{-0.5f, -0.5f,  0.5f}, {0, -1, 0}, {0, 0}, 0xFFFFFFFF},

            // Right face (X+)
            {{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 1}, 0xFFFFFFFF},
            {{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 1}, 0xFFFFFFFF},
            {{ 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {1, 0}, 0xFFFFFFFF},
            {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 0}, 0xFFFFFFFF},

            // Left face (X-)
            {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}, 0xFFFFFFFF},
            {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 1}, 0xFFFFFFFF},
            {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 0}, 0xFFFFFFFF},
            {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 0}, 0xFFFFFFFF},
        };

        uint32_t indices[] = {
            0, 1, 2, 2, 3, 0,       // Front
            4, 5, 6, 6, 7, 4,       // Back
            8, 9, 10, 10, 11, 8,   // Top
            12, 13, 14, 14, 15, 12, // Bottom
            16, 17, 18, 18, 19, 16, // Right
            20, 21, 22, 22, 23, 20  // Left
        };

        m_cubeVertexBuffer = m_renderer->createBuffer(
            BufferType::Vertex,
            vertices,
            sizeof(vertices)
        );

        m_cubeIndexBuffer = m_renderer->createBuffer(
            BufferType::Index,
            indices,
            sizeof(indices)
        );
    }
};

} // namespace opensaints

// Demo entry point
int runDemo() {
    using namespace opensaints;

    std::cout << "OpenSaints Demo Mode\n";
    std::cout << "Controls: WASD to move, mouse to look (click to capture)\n";
    std::cout << "Press ESC to release mouse, close window to exit\n\n";

    // Create application
    Application app;
    WindowConfig config;
    config.title = "OpenSaints Demo";
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

    // Create demo scene
    DemoScene demo;
    if (!demo.initialize(&app, renderer.get())) {
        std::cerr << "Failed to initialize demo scene\n";
        return 1;
    }

    // Set callbacks
    app.setUpdateCallback([&demo](float dt) {
        demo.update(dt);
    });

    app.setRenderCallback([&demo, &renderer]() {
        if (renderer->beginFrame()) {
            renderer->clear(Color::cornflowerBlue());
            demo.render();
            renderer->endFrame();
        }
    });

    app.setResizeCallback([&renderer](int w, int h) {
        renderer->onResize(w, h);
    });

    // Run main loop
    app.run();

    // Cleanup
    demo.shutdown();
    renderer->shutdown();
    app.shutdown();

    return 0;
}
