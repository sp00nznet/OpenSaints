# OpenSaints engine library

# Format parsers
set(FORMAT_SOURCES
    formats/vpp.cpp
    formats/vpp.h
    formats/preload_table.cpp
    formats/preload_table.h
    formats/xtbl.cpp
    formats/xtbl.h
    formats/peg.cpp
    formats/peg.h
    formats/mesh.cpp
    formats/mesh.h
    formats/chunk.cpp
    formats/chunk.h
    formats/anim.cpp
    formats/anim.h
)

# Engine core
set(ENGINE_SOURCES
    engine/vfs.cpp
    engine/vfs.h
    engine/asset_manager.cpp
    engine/asset_manager.h
    engine/entity.cpp
    engine/entity.h
    engine/animation.cpp
    engine/animation.h
)

# World/streaming
set(WORLD_SOURCES
    world/streaming.cpp
    world/streaming.h
)

# Audio
set(AUDIO_SOURCES
    audio/audio_system.cpp
    audio/audio_system.h
)

# UI
set(UI_SOURCES
    ui/vint_doc.cpp
    ui/vint_doc.h
)

# Script
set(SCRIPT_SOURCES
    script/script_system.cpp
    script/script_system.h
)

# Physics
set(PHYSICS_SOURCES
    physics/physics_system.cpp
    physics/physics_system.h
)

# Platform layer (SDL2)
set(PLATFORM_SOURCES
    platform/application.cpp
    platform/application.h
)

# Rendering
set(RENDER_SOURCES
    render/renderer.cpp
    render/renderer.h
    render/vulkan_backend.cpp
    render/vulkan_backend.h
)

# Combine all sources
add_library(opensaints_core STATIC
    ${FORMAT_SOURCES}
    ${ENGINE_SOURCES}
    ${WORLD_SOURCES}
    ${AUDIO_SOURCES}
    ${UI_SOURCES}
    ${SCRIPT_SOURCES}
    ${PHYSICS_SOURCES}
    ${PLATFORM_SOURCES}
    ${RENDER_SOURCES}
)

target_include_directories(opensaints_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable threading support
find_package(Threads REQUIRED)
target_link_libraries(opensaints_core PUBLIC Threads::Threads)

# Platform-specific libraries
if(WIN32)
    # Windows-specific
elseif(APPLE)
    # macOS-specific
elseif(UNIX)
    # Linux-specific
    target_link_libraries(opensaints_core PUBLIC stdc++fs)
endif()

# Optional SDL2 support
if(SDL2_FOUND)
    target_include_directories(opensaints_core PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(opensaints_core PUBLIC ${SDL2_LIBRARIES})
    target_compile_definitions(opensaints_core PUBLIC HAVE_SDL2)
endif()

# Optional Vulkan support
if(Vulkan_FOUND)
    target_include_directories(opensaints_core PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_include_directories(opensaints_core PUBLIC ${CMAKE_SOURCE_DIR}/external)
    # Use volk for dynamic Vulkan loading
    target_sources(opensaints_core PRIVATE ${CMAKE_SOURCE_DIR}/external/volk.c)
    target_compile_definitions(opensaints_core PUBLIC HAVE_VULKAN VK_NO_PROTOTYPES)
endif()

# Optional GLM support
if(GLM_INCLUDE_DIR)
    target_include_directories(opensaints_core PUBLIC ${GLM_INCLUDE_DIR})
    target_compile_definitions(opensaints_core PUBLIC HAVE_GLM)
endif()

# Optional OpenAL support
if(OPENAL_FOUND)
    target_include_directories(opensaints_core PUBLIC ${OPENAL_INCLUDE_DIR})
    target_link_libraries(opensaints_core PUBLIC ${OPENAL_LIBRARY})
    target_compile_definitions(opensaints_core PUBLIC HAVE_OPENAL)
endif()

# Main executable
add_executable(opensaints
    main.cpp
)

target_link_libraries(opensaints PRIVATE opensaints_core)

# Demo executable (when renderer is available)
if(BUILD_RENDERER AND SDL2_FOUND AND Vulkan_FOUND)
    add_executable(opensaints_demo
        demo.cpp
    )
    target_link_libraries(opensaints_demo PRIVATE opensaints_core)
endif()

# Asset test tool (always built)
add_executable(asset_test
    asset_test.cpp
)
target_link_libraries(asset_test PRIVATE opensaints_core)
