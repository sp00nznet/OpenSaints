#include "application.h"

#ifdef HAVE_SDL2
#include <SDL.h>
#include <iostream>

namespace opensaints {

Application* Application::s_instance = nullptr;

Application::Application() {
    s_instance = this;
}

Application::~Application() {
    shutdown();
    s_instance = nullptr;
}

bool Application::initialize(const WindowConfig& config) {
    if (m_initialized) {
        return true;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return false;
    }

    // Create window with Vulkan support
    uint32_t windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN;
    if (config.resizable) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    m_window = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.width,
        config.height,
        windowFlags
    );

    if (!m_window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    m_width = config.width;
    m_height = config.height;
    m_initialized = true;
    m_running = true;
    m_lastFrameTime = SDL_GetPerformanceCounter();

    // Open any connected game controllers
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* controller = SDL_GameControllerOpen(i);
            if (controller) {
                m_input.gamepad_connected = true;
                std::cout << "Gamepad connected: " << SDL_GameControllerName(controller) << "\n";
                break; // Just use first controller for now
            }
        }
    }

    std::cout << "Application initialized: " << m_width << "x" << m_height << "\n";
    return true;
}

void Application::shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
    m_initialized = false;
    m_running = false;

    std::cout << "Application shutdown\n";
}

void Application::run() {
    while (m_running) {
        resetFrameInput();
        processEvents();
        updateTiming();

        if (m_updateCallback) {
            m_updateCallback(m_deltaTime);
        }

        if (m_renderCallback) {
            m_renderCallback();
        }
    }
}

void Application::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handleEvent(event);
    }
}

void Application::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            m_running = false;
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                m_width = event.window.data1;
                m_height = event.window.data2;
                if (m_resizeCallback) {
                    m_resizeCallback(m_width, m_height);
                }
            }
            break;

        case SDL_KEYDOWN:
            if (event.key.keysym.scancode < 512) {
                if (!m_input.keys[event.key.keysym.scancode]) {
                    m_input.keys_pressed[event.key.keysym.scancode] = true;
                }
                m_input.keys[event.key.keysym.scancode] = true;
            }
            // Escape to release mouse
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE && m_input.mouse_captured) {
                captureMouse(false);
            }
            break;

        case SDL_KEYUP:
            if (event.key.keysym.scancode < 512) {
                m_input.keys[event.key.keysym.scancode] = false;
                m_input.keys_released[event.key.keysym.scancode] = true;
            }
            break;

        case SDL_MOUSEMOTION:
            m_input.mouse_x = event.motion.x;
            m_input.mouse_y = event.motion.y;
            m_input.mouse_delta_x += event.motion.xrel;
            m_input.mouse_delta_y += event.motion.yrel;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button <= 5) {
                m_input.mouse_buttons[event.button.button - 1] = true;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button <= 5) {
                m_input.mouse_buttons[event.button.button - 1] = false;
            }
            break;

        case SDL_MOUSEWHEEL:
            m_input.mouse_wheel += event.wheel.y;
            break;

        case SDL_CONTROLLERAXISMOTION:
            switch (event.caxis.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    m_input.gamepad_left_x = event.caxis.value / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    m_input.gamepad_left_y = event.caxis.value / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    m_input.gamepad_right_x = event.caxis.value / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    m_input.gamepad_right_y = event.caxis.value / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                    m_input.gamepad_triggers[0] = event.caxis.value / 32767.0f;
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                    m_input.gamepad_triggers[1] = event.caxis.value / 32767.0f;
                    break;
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            if (event.cbutton.button < 16) {
                m_input.gamepad_buttons[event.cbutton.button] = true;
            }
            break;

        case SDL_CONTROLLERBUTTONUP:
            if (event.cbutton.button < 16) {
                m_input.gamepad_buttons[event.cbutton.button] = false;
            }
            break;

        case SDL_CONTROLLERDEVICEADDED:
            if (!m_input.gamepad_connected) {
                SDL_GameController* controller = SDL_GameControllerOpen(event.cdevice.which);
                if (controller) {
                    m_input.gamepad_connected = true;
                    std::cout << "Gamepad connected: " << SDL_GameControllerName(controller) << "\n";
                }
            }
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            m_input.gamepad_connected = false;
            std::cout << "Gamepad disconnected\n";
            break;
    }
}

void Application::updateTiming() {
    uint64_t currentTime = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();

    m_deltaTime = static_cast<float>(currentTime - m_lastFrameTime) / frequency;
    m_lastFrameTime = currentTime;

    // Clamp delta time to avoid spiral of death
    if (m_deltaTime > 0.25f) {
        m_deltaTime = 0.25f;
    }

    m_totalTime += m_deltaTime;
    ++m_frameCount;

    // FPS calculation (update every second)
    m_fpsAccumulator += m_deltaTime;
    ++m_fpsFrameCount;
    if (m_fpsAccumulator >= 1.0f) {
        m_fps = m_fpsFrameCount / m_fpsAccumulator;
        m_fpsAccumulator = 0;
        m_fpsFrameCount = 0;
    }
}

void Application::resetFrameInput() {
    // Clear per-frame input state
    std::fill(std::begin(m_input.keys_pressed), std::end(m_input.keys_pressed), false);
    std::fill(std::begin(m_input.keys_released), std::end(m_input.keys_released), false);
    m_input.mouse_delta_x = 0;
    m_input.mouse_delta_y = 0;
    m_input.mouse_wheel = 0;
}

void Application::captureMouse(bool capture) {
    m_input.mouse_captured = capture;
    SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
}

} // namespace opensaints

#else // No SDL2

#include <iostream>

namespace opensaints {

Application* Application::s_instance = nullptr;

Application::Application() { s_instance = this; }
Application::~Application() { s_instance = nullptr; }

bool Application::initialize(const WindowConfig&) {
    std::cerr << "SDL2 not available. Build with -DBUILD_RENDERER=ON and install SDL2.\n";
    return false;
}

void Application::shutdown() {}
void Application::run() {}
void Application::captureMouse(bool) {}

} // namespace opensaints

#endif // HAVE_SDL2
