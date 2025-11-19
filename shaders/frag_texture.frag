#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec2 tileOrigin;      // where tile starts in atlas (0–1)
layout(location = 3) in vec2 tileSize;        // tile size in atlas (0–1)
layout(location = 4) in vec2 repeatCount;     // how many repeats inside the tile

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 localUV = fract(fragUV * repeatCount);
    vec2 atlasUV = tileOrigin + localUV * tileSize;

    vec4 texel = texture(texSampler, atlasUV);
    outColor = texel * fragColor[3];
}
