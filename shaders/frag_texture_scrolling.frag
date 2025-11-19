#version 450

layout(push_constant, std430) uniform FragPushConstant {
    layout(offset = 64) vec2 atlasOffset;
    vec2 atlasScale;
    vec2 cameraWorldPos;
    float globalTime;
} pc;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec2 tileOrigin;      // where tile starts in atlas (0–1)
layout(location = 3) in vec2 tileSize;        // tile size in atlas (0–1)
layout(location = 4) in vec2 repeatCount;     // how many repeats inside the tile

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float scroll = fract(pc.globalTime * 0.000025); 

    // Local UV inside the tile (0–1)
    vec2 localUv = fract(fragUV * repeatCount);
    localUv = tileOrigin + localUv * tileSize;
    
    // localUv.y += scroll;
    float val = localUv.y + scroll;
    localUv.y = pc.atlasOffset.y + mod(val - pc.atlasOffset.y, pc.atlasScale.y);

    vec4 texel = texture(texSampler, localUv);
    outColor = texel * fragColor;
}
