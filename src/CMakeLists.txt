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
)

# Engine core
set(ENGINE_SOURCES
    engine/vfs.cpp
    engine/vfs.h
    engine/asset_manager.cpp
    engine/asset_manager.h
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
    target_link_libraries(opensaints_core PUBLIC ${Vulkan_LIBRARIES})
    target_compile_definitions(opensaints_core PUBLIC HAVE_VULKAN)
endif()

# Optional GLM support
if(GLM_INCLUDE_DIR)
    target_include_directories(opensaints_core PUBLIC ${GLM_INCLUDE_DIR})
    target_compile_definitions(opensaints_core PUBLIC HAVE_GLM)
endif()

# Main executable
add_executable(opensaints
    main.cpp
)

target_link_libraries(opensaints PRIVATE opensaints_core)
