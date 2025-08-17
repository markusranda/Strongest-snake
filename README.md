## Prerequisites

- **CMake ≥ 3.15**
- **Clang or MSVC** (tested with MSVC 2022)
- **vcpkg** checked out in `C:\vcpkg`

## Download dependencies

```
C:\vcpkg\vcpkg.exe install --triplet x64-windows
```

### prepare buil

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug

### compile game

cmake --build build --config Debug

### run game

.\build\Debug\strongest_snake.exe
