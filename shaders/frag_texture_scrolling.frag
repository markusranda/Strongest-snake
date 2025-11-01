#version 450

layout(push_constant, std430) uniform FragPushConstant {
    layout(offset = 64) vec2 atlasOffset;  
    vec2 atlasScale;    
    float globalTime;       
} pc;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float scroll = fract(pc.globalTime * 0.0000025); 

    // Local UV inside the tile (0–1)
    vec2 localUv = fragUV;
    // localUv.y += scroll;
    float val = localUv.y + scroll;
    localUv.y = pc.atlasOffset.y + mod(val - pc.atlasOffset.y, pc.atlasScale.y);

    vec4 texel = texture(texSampler, localUv);
    outColor = texel * fragColor;
}
