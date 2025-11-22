#version 450

layout(push_constant) uniform Push {
    mat4 viewProj;
} push;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in mat4 instanceModel;
layout(location = 6) in vec4 instanceColor;
layout(location = 7) in vec4 instanceUV;
layout(location = 8) in vec2 worldSize; // I needed vulkan to shut the fuck up

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    gl_Position = instanceModel * vec4(inPos, 0.0, 1.0);
    fragColor = instanceColor;
    fragUV = instanceUV.xy + inUV * instanceUV.zw;
}
