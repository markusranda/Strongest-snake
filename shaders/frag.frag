#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

bool isEdge() {
    float edgeThreshold = 0.1; // how thick the edge band is

    // distance to nearest edge
    float distToEdge = min(
        min(fragUV.x, 1.0 - fragUV.x),
        min(fragUV.y, 1.0 - fragUV.y)
    );

    return step(distToEdge, edgeThreshold) == 1;
}

void main() {
    if (isEdge()) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        outColor = fragColor;
    }
}
