# Contributing to OpenSaints

Thank you for your interest in contributing to OpenSaints! This document provides guidelines for contributing to the project.

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help newcomers learn

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a feature branch
4. Make your changes
5. Submit a pull request

```bash
git clone https://github.com/yourusername/OpenSaints.git
cd OpenSaints
git checkout -b feature/my-feature
```

## What to Contribute

### High Priority

- **Format reverse engineering** - Help document unknown file formats
- **Vulkan renderer** - Implement rendering backend
- **Cross-platform testing** - Test on Linux, macOS
- **Documentation** - Improve docs and comments

### Good First Issues

Look for issues labeled `good first issue`:
- Documentation improvements
- Code cleanup
- Bug fixes in existing parsers
- Test coverage

### Areas Needing Help

| Area | Skills Needed |
|------|---------------|
| Renderer | Vulkan, graphics programming |
| Audio | OpenAL, audio programming |
| Formats | Binary reverse engineering |
| Physics | Physics engine integration |
| Testing | C++, Python |

## Code Style

### C++ Guidelines

```cpp
// Use descriptive names
class AssetManager;        // Good
class AM;                  // Bad

// Use camelCase for functions and variables
void loadTexture();
int textureCount;

// Use PascalCase for types
struct TextureAsset;
enum class AssetType;

// Use m_ prefix for member variables
class Example {
    int m_count;
    std::string m_name;
};

// Use snake_case for file names
// asset_manager.cpp, asset_manager.h

// Braces on same line for functions
void example() {
    if (condition) {
        // code
    }
}
```

### Header Files

```cpp
#pragma once
// Brief description of the file

#include <standard_headers>
#include "project_headers.h"

namespace opensaints {

// Forward declarations first
class ForwardDeclared;

// Then class definitions
class MyClass {
public:
    // Public interface

private:
    // Implementation details
};

} // namespace opensaints
```

### Comments

```cpp
// Use // for single-line comments

/*
 * Use block comments for longer explanations
 * spanning multiple lines
 */

/// Use /// for documentation comments (Doxygen-style)
/// @param name Parameter description
/// @return Return value description
```

## Commit Messages

Format:
```
<type>: <subject>

<body>

<footer>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `refactor`: Code refactoring
- `test`: Adding tests
- `chore`: Maintenance

Examples:
```
feat: Add PEG texture parsing

Implement parsing of .cpeg_pc/.gpeg_pc texture packages
with DXT1/3/5 decompression support.

Closes #42
```

```
fix: Handle empty VPP archives gracefully

Previously, opening a VPP with 0 files would crash.
Now returns false from open() with appropriate error.
```

## Pull Request Process

1. **Update documentation** - Add/update docs for new features
2. **Add tests** - Include tests for new functionality
3. **Follow style guide** - Ensure code matches project style
4. **One feature per PR** - Keep PRs focused and reviewable
5. **Describe changes** - Write clear PR description

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
How was this tested?

## Checklist
- [ ] Code follows style guide
- [ ] Documentation updated
- [ ] Tests added/updated
- [ ] All tests pass
```

## Clean Room Guidelines

**IMPORTANT:** This is a clean-room reimplementation. To maintain legal safety:

### DO
- Reference public format documentation
- Use community research and specs
- Write original code based on format understanding
- Study file structures with hex editors

### DO NOT
- Copy code from other SR2 tools (Gibbed, ThomasJepp, etc.)
- Decompile and copy original game code
- Use leaked source code or assets
- Include proprietary algorithms

### When Documenting Formats

1. Describe what you observe, not how original code works
2. Reference public documentation sources
3. Note "unknown" fields rather than guessing from decompiled code
4. Include sources/references

## Testing

### Running Tests

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Writing Tests

```cpp
// tests/test_vpp.cpp
#include <gtest/gtest.h>
#include "formats/vpp.h"

TEST(VppArchive, OpenValidArchive) {
    opensaints::VppArchive archive;
    EXPECT_TRUE(archive.open("test_data/test.vpp_pc"));
    EXPECT_GT(archive.fileCount(), 0);
}

TEST(VppArchive, RejectInvalidMagic) {
    opensaints::VppArchive archive;
    EXPECT_FALSE(archive.open("test_data/not_a_vpp.bin"));
}
```

## Reporting Issues

### Bug Reports

Include:
- Steps to reproduce
- Expected behavior
- Actual behavior
- System info (OS, compiler)
- Relevant log output

### Feature Requests

Include:
- Use case description
- Proposed solution
- Alternatives considered

## Development Workflow

```
main (stable)
  │
  ├── develop (integration)
  │     │
  │     ├── feature/renderer
  │     ├── feature/audio
  │     └── fix/vpp-crash
  │
  └── release/v0.2.0
```

1. Create feature branch from `develop`
2. Implement and test
3. Submit PR to `develop`
4. After review, merge to `develop`
5. Periodically, `develop` merges to `main` for releases

## Questions?

- Open a GitHub Discussion
- Check existing issues
- Read the documentation

Thank you for contributing!
