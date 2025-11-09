#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

bool isEdge() {
    float edgeThreshold = 0.005;
    float distToEdge = min(
        min(fragUV.x, 1.0 - fragUV.x),
        min(fragUV.y, 1.0 - fragUV.y)
    );
    return distToEdge < edgeThreshold;
}

void main() {
    if (isEdge()) {
        outColor = vec4(1.0, 0.0, 0.0, 0.6); // semi-transparent red border
    } else {
        discard;
    }
}
