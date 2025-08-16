## Build and go

```
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
clang++ win32_window.cpp -o win32_window.exe -lgdi32 -luser32
.\build\strongest_snake.exe
```
