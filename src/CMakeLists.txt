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

# Combine all sources
add_library(opensaints_core STATIC
    ${FORMAT_SOURCES}
    ${ENGINE_SOURCES}
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

# Main executable
add_executable(opensaints
    main.cpp
)

target_link_libraries(opensaints PRIVATE opensaints_core)
