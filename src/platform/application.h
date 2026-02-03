#pragma once
// Application framework for OpenSaints
// Handles window creation, input, and main loop using SDL2

#include <cstdint>
#include <string>
#include <functional>
#include <memory>

// Forward declare SDL types to avoid including SDL in header
struct SDL_Window;
union SDL_Event;

namespace opensaints {

// Window configuration
struct WindowConfig {
    std::string title = "OpenSaints";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
};

// Input state
struct InputState {
    // Keyboard
    bool keys[512] = {false};
    bool keys_pressed[512] = {false};   // Just pressed this frame
    bool keys_released[512] = {false};  // Just released this frame

    // Mouse
    int mouse_x = 0;
    int mouse_y = 0;
    int mouse_delta_x = 0;
    int mouse_delta_y = 0;
    int mouse_wheel = 0;
    bool mouse_buttons[5] = {false};
    bool mouse_captured = false;

    // Gamepad (simplified)
    float gamepad_left_x = 0;
    float gamepad_left_y = 0;
    float gamepad_right_x = 0;
    float gamepad_right_y = 0;
    float gamepad_triggers[2] = {0, 0};
    bool gamepad_buttons[16] = {false};
    bool gamepad_connected = false;
};

// Key codes (subset of SDL scancodes)
namespace Key {
    constexpr int Unknown = 0;
    constexpr int A = 4;
    constexpr int B = 5;
    constexpr int C = 6;
    constexpr int D = 7;
    constexpr int E = 8;
    constexpr int F = 9;
    constexpr int G = 10;
    constexpr int H = 11;
    constexpr int I = 12;
    constexpr int J = 13;
    constexpr int K = 14;
    constexpr int L = 15;
    constexpr int M = 16;
    constexpr int N = 17;
    constexpr int O = 18;
    constexpr int P = 19;
    constexpr int Q = 20;
    constexpr int R = 21;
    constexpr int S = 22;
    constexpr int T = 23;
    constexpr int U = 24;
    constexpr int V = 25;
    constexpr int W = 26;
    constexpr int X = 27;
    constexpr int Y = 28;
    constexpr int Z = 29;
    constexpr int Num1 = 30;
    constexpr int Num2 = 31;
    constexpr int Num3 = 32;
    constexpr int Num4 = 33;
    constexpr int Num5 = 34;
    constexpr int Num6 = 35;
    constexpr int Num7 = 36;
    constexpr int Num8 = 37;
    constexpr int Num9 = 38;
    constexpr int Num0 = 39;
    constexpr int Return = 40;
    constexpr int Escape = 41;
    constexpr int Backspace = 42;
    constexpr int Tab = 43;
    constexpr int Space = 44;
    constexpr int F1 = 58;
    constexpr int F2 = 59;
    constexpr int F3 = 60;
    constexpr int F4 = 61;
    constexpr int F5 = 62;
    constexpr int F6 = 63;
    constexpr int F7 = 64;
    constexpr int F8 = 65;
    constexpr int F9 = 66;
    constexpr int F10 = 67;
    constexpr int F11 = 68;
    constexpr int F12 = 69;
    constexpr int Right = 79;
    constexpr int Left = 80;
    constexpr int Down = 81;
    constexpr int Up = 82;
    constexpr int LCtrl = 224;
    constexpr int LShift = 225;
    constexpr int LAlt = 226;
    constexpr int RCtrl = 228;
    constexpr int RShift = 229;
    constexpr int RAlt = 230;
}

// Mouse buttons
namespace MouseButton {
    constexpr int Left = 0;
    constexpr int Middle = 1;
    constexpr int Right = 2;
    constexpr int X1 = 3;
    constexpr int X2 = 4;
}

// Application callbacks
using UpdateCallback = std::function<void(float deltaTime)>;
using RenderCallback = std::function<void()>;
using ResizeCallback = std::function<void(int width, int height)>;

// Main application class
class Application {
public:
    Application();
    ~Application();

    // Initialize SDL and create window
    bool initialize(const WindowConfig& config = WindowConfig{});

    // Shutdown and cleanup
    void shutdown();

    // Run the main loop
    void run();

    // Request quit
    void quit() { m_running = false; }

    // Check if running
    bool isRunning() const { return m_running; }

    // Set callbacks
    void setUpdateCallback(UpdateCallback callback) { m_updateCallback = callback; }
    void setRenderCallback(RenderCallback callback) { m_renderCallback = callback; }
    void setResizeCallback(ResizeCallback callback) { m_resizeCallback = callback; }

    // Get input state
    const InputState& input() const { return m_input; }

    // Input helpers
    bool isKeyDown(int key) const { return m_input.keys[key]; }
    bool isKeyPressed(int key) const { return m_input.keys_pressed[key]; }
    bool isKeyReleased(int key) const { return m_input.keys_released[key]; }
    bool isMouseButtonDown(int button) const { return m_input.mouse_buttons[button]; }

    // Mouse capture (for FPS-style controls)
    void captureMouse(bool capture);
    bool isMouseCaptured() const { return m_input.mouse_captured; }

    // Window info
    int windowWidth() const { return m_width; }
    int windowHeight() const { return m_height; }
    float aspectRatio() const { return static_cast<float>(m_width) / m_height; }

    // Get SDL window (for Vulkan surface creation)
    SDL_Window* getSDLWindow() const { return m_window; }

    // Timing
    float deltaTime() const { return m_deltaTime; }
    float totalTime() const { return m_totalTime; }
    uint64_t frameCount() const { return m_frameCount; }
    float fps() const { return m_fps; }

    // Get instance
    static Application* instance() { return s_instance; }

private:
    static Application* s_instance;

    SDL_Window* m_window = nullptr;
    bool m_running = false;
    bool m_initialized = false;

    // Window state
    int m_width = 0;
    int m_height = 0;

    // Input state
    InputState m_input;

    // Timing
    float m_deltaTime = 0;
    float m_totalTime = 0;
    uint64_t m_frameCount = 0;
    float m_fps = 0;
    uint64_t m_lastFrameTime = 0;
    float m_fpsAccumulator = 0;
    int m_fpsFrameCount = 0;

    // Callbacks
    UpdateCallback m_updateCallback;
    RenderCallback m_renderCallback;
    ResizeCallback m_resizeCallback;

    // Internal methods
    void processEvents();
    void handleEvent(const SDL_Event& event);
    void updateTiming();
    void resetFrameInput();
};

} // namespace opensaints
