#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    vec2  centerPx; // pixels
    float featherPx;  // pixels
    float radiusPx;   // pixels
} push;

void main()
{
    float distPx = length(gl_FragCoord.xy - push.centerPx);
    float alpha  = smoothstep(push.radiusPx, push.radiusPx + max(push.featherPx, 0.0001), distPx);
    outColor = vec4(0,0,0, alpha);
}
