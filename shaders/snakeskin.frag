#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

float circle(vec2 uv, vec2 center, float radius) {
    return step(length(uv - center), radius);
}

bool isEdge() {
    float edgeThreshold = 0.05; // how thick the edge band is

    // distance to nearest edge
    float distToEdge = min(
        min(fragUV.x, 1.0 - fragUV.x),
        min(fragUV.y, 1.0 - fragUV.y)
    );

    return step(distToEdge, edgeThreshold) == 1;
}

void main() {
    // Three hardcoded spots
    float s1 = circle(fragUV, vec2(0.3, 0.3), 0.1);
    float s2 = circle(fragUV, vec2(0.7, 0.4), 0.08);
    float s3 = circle(fragUV, vec2(0.5, 0.7), 0.12);

    float spots = max(s1, max(s2, s3));

    vec3 base = fragColor.rgb;
    vec3 spotColor = vec3(1.0, 0.0, 0.0);

    if (isEdge()) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        outColor = vec4(mix(base, spotColor, spots), 1.0);
    }
}
