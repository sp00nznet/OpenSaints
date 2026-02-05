cmake_minimum_required(VERSION 3.16)
project(OpenSaints VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TOOLS "Build asset extraction tools" ON)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_RENDERER "Build rendering engine (requires SDL2, Vulkan)" OFF)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

# Find external dependencies (optional for Phase 2)
if(BUILD_RENDERER)
    # SDL2
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        message(STATUS "Found SDL2: ${SDL2_VERSION}")
        add_definitions(-DHAVE_SDL2)
    else()
        message(WARNING "SDL2 not found. Install SDL2 development libraries.")
    endif()

    # Vulkan
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Found Vulkan: ${Vulkan_VERSION}")
        add_definitions(-DHAVE_VULKAN)
    else()
        message(WARNING "Vulkan not found. Install Vulkan SDK.")
    endif()

    # GLM (header-only math library)
    find_path(GLM_INCLUDE_DIR glm/glm.hpp
        HINTS
            ${CMAKE_SOURCE_DIR}/external/glm
            /usr/include
            /usr/local/include
    )
    if(GLM_INCLUDE_DIR)
        message(STATUS "Found GLM: ${GLM_INCLUDE_DIR}")
        add_definitions(-DHAVE_GLM)
    else()
        message(WARNING "GLM not found. Install GLM or add to external/glm.")
    endif()

    # OpenAL (3D audio)
    find_package(OpenAL QUIET)
    if(OPENAL_FOUND)
        message(STATUS "Found OpenAL: ${OPENAL_INCLUDE_DIR}")
        add_definitions(-DHAVE_OPENAL)
    else()
        message(WARNING "OpenAL not found. Install OpenAL Soft for audio support.")
    endif()
endif()

# Source files
add_subdirectory(src)

if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "OpenSaints ${PROJECT_VERSION} Configuration:")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build tools: ${BUILD_TOOLS}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build renderer: ${BUILD_RENDERER}")
message(STATUS "")
