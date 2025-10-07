#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

bool isEdge() {
    float edgeThreshold = 0.025; // how thick the edge band is

    // distance to nearest edge
    float distToEdge = min(
        min(fragUV.x, 1.0 - fragUV.x),
        min(fragUV.y, 1.0 - fragUV.y)
    );

    return step(distToEdge, edgeThreshold) == 1;
}

bool isHinge() {
    return fragUV.x < 0.1 && fragUV.y > 0.45 && fragUV.y < 0.55;
}

bool isArrowTime() {
    return fragUV.x > 0.5 && fragUV.y > 0.4 && fragUV.y < 0.6;
}

void main() {
    if (isHinge()) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
    else if (isArrowTime()) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else if (isEdge()) {
        outColor = vec4(0.0, 0.0, 0.0, 0.2);
    } else {
        outColor = fragColor;
    }
}
