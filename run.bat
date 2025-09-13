glslc .\shaders\shader.vert -o shaders/vert.spv
glslc .\shaders\shader.frag -o shaders/frag.spv
glslc .\shaders\shader.comp -o shaders/comp.spv

cmake --build build --config Debug
.\build\Debug\strongest_snake.exe