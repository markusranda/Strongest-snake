#version 450

layout(push_constant) uniform Camera {
    mat4 viewProj;
} camera;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in mat4 instanceModel;
layout(location = 6) in vec4 instanceColor;
layout(location = 7) in vec4 instanceUV; 
layout(location = 8) in vec2 worldSize; 

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec2 tileOrigin;      // where tile starts in atlas (0–1)
layout(location = 3) out vec2 tileSize;        // tile size in atlas (0–1)
layout(location = 4) out vec2 repeatCount;     // how many repeats inside the tile

void main() {
    gl_Position = camera.viewProj * instanceModel * vec4(inPos, 0.0, 1.0);
    fragColor = instanceColor;
    fragUV = inUV;

    tileOrigin = instanceUV.xy;
    tileSize = instanceUV.zw;
    repeatCount = worldSize / 32.0; // TODO This should probably be more dynamic, but 32.0 is the size of one tile in pixels 
}
