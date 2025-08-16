cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
.\build\strongest_snake.exe
