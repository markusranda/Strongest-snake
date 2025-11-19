#version 450

layout(push_constant, std430) uniform FragPushConstant {
    layout(offset = 64) vec2 atlasOffset;
    vec2 atlasScale;
    vec2 cameraWorldPos;
    float globalTime;
} pc;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;           // raw 0–1 quad UVs
layout(location = 2) in vec2 tileOrigin;       // UV of atlas tile origin
layout(location = 3) in vec2 tileSize;         // UV size of the tile in atlas
layout(location = 4) in vec2 repeatCount;      // number of repeats across quad

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    float parallaxStrength = 0.5f;
    float wrapLength = 50;
    vec2 cameraUVOffset = pc.cameraWorldPos / wrapLength;

    // apply parallax offset and repeat
    vec2 localUV = fract(fragUV * repeatCount + cameraUVOffset * parallaxStrength);

    // convert tile-local UV into atlas UV
    vec2 atlasUV = tileOrigin + localUV * tileSize;

    vec4 texel = texture(texSampler, atlasUV);
    outColor = texel * fragColor.a;
}
