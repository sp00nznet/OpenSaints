# Building OpenSaints

This guide covers building OpenSaints from source on various platforms.

## Prerequisites

### All Platforms
- CMake 3.16 or newer
- C++20 compatible compiler
- Git

### Windows
- Visual Studio 2019 or newer (with C++ workload)
- Or: MinGW-w64 with GCC 10+

### Linux
- GCC 10+ or Clang 10+
- Development packages: `build-essential cmake`

### macOS
- Xcode Command Line Tools
- Or: Homebrew with `brew install cmake gcc`

## Basic Build

### Clone and Build

```bash
# Clone the repository
git clone https://github.com/yourusername/OpenSaints.git
cd OpenSaints

# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build . --config Release
```

### Windows (Visual Studio)

```powershell
# Generate Visual Studio solution
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build from command line
cmake --build build --config Release

# Or open build/OpenSaints.sln in Visual Studio
```

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TOOLS` | ON | Build extraction tools |
| `BUILD_TESTS` | OFF | Build unit tests |
| `BUILD_RENDERER` | OFF | Build Vulkan renderer |
| `CMAKE_BUILD_TYPE` | Debug | Debug/Release/RelWithDebInfo |

### Examples

```bash
# Build with renderer support
cmake -B build -DBUILD_RENDERER=ON

# Build everything including tests
cmake -B build -DBUILD_TOOLS=ON -DBUILD_TESTS=ON

# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

## Dependencies for Renderer

To build with `BUILD_RENDERER=ON`, you need:

### SDL2

**Windows:**
- Download from https://libsdl.org/download-2.0.php
- Extract to `C:\SDL2` or set `SDL2_DIR`

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel

# Arch
sudo pacman -S sdl2
```

**macOS:**
```bash
brew install sdl2
```

### Vulkan SDK

**All Platforms:**
1. Download from https://vulkan.lunarg.com/
2. Run installer
3. Ensure `VULKAN_SDK` environment variable is set

**Linux (Alternative):**
```bash
# Ubuntu/Debian
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers

# Fedora
sudo dnf install vulkan-tools vulkan-loader-devel vulkan-validation-layers
```

### GLM

GLM is header-only and can be:

**Option 1: System Install**
```bash
# Ubuntu/Debian
sudo apt install libglm-dev

# Fedora
sudo dnf install glm-devel

# macOS
brew install glm
```

**Option 2: Local Copy**
```bash
# Download to external/glm
mkdir -p external
cd external
git clone https://github.com/g-truc/glm.git
```

## IDE Setup

### Visual Studio

1. Open folder in VS (File > Open > Folder)
2. VS will detect CMakeLists.txt
3. Select configuration from dropdown
4. Build with Ctrl+Shift+B

### Visual Studio Code

1. Install extensions: C/C++, CMake, CMake Tools
2. Open folder
3. CMake Tools will auto-configure
4. Press F7 to build

### CLion

1. Open folder as CMake project
2. CLion handles everything automatically
3. Build with Ctrl+F9

## Troubleshooting

### CMake can't find SDL2

Set the SDL2 directory:
```bash
cmake -B build -DSDL2_DIR=/path/to/SDL2
```

### Vulkan not found

Ensure VULKAN_SDK is set:
```bash
# Windows PowerShell
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.xxx"

# Linux/macOS
export VULKAN_SDK=/path/to/vulkan/sdk
```

### C++20 not supported

Update your compiler:
- MSVC: Update Visual Studio
- GCC: `sudo apt install g++-10` (use `-DCMAKE_CXX_COMPILER=g++-10`)
- Clang: `sudo apt install clang-10`

### Linux filesystem library

If you get linker errors about `std::filesystem`:
```bash
# Add to CMakeLists.txt or command line
cmake -B build -DCMAKE_CXX_FLAGS="-lstdc++fs"
```

## Output Files

After building:

```
build/
├── opensaints           # Main executable
├── vpp_extract          # VPP extraction tool
└── opensaints_core.a    # Static library (or .lib on Windows)
```

## Running

```bash
# Show help
./build/opensaints --help

# Point to your Saints Row 2 installation
./build/opensaints "C:/Games/Saints Row 2"
./build/opensaints "/home/user/.steam/steam/steamapps/common/Saints Row 2"
```

## Development Build

For development, use Debug mode with extra warnings:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
```

## Continuous Integration

The project includes CI configuration for:
- GitHub Actions (Windows, Linux, macOS)
- Build verification on pull requests

See `.github/workflows/` for details.
