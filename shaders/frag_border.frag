#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    float borderThickness = 0.25;
    float dist = min(min(fragUV.x, 1.0 - fragUV.x), min(fragUV.y, 1.0 - fragUV.y));

    float pxWidth = fwidth(dist);

    float maxPx = 4.0; 
    float edge = smoothstep((borderThickness * pxWidth) * maxPx, pxWidth * maxPx, dist);
    vec4 color = vec4(1.0, 0.0, 0.0, 0.6);

    outColor = mix(color, vec4(0.0), edge);
}
