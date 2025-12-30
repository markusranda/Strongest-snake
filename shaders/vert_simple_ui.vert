#version 450

layout(push_constant) uniform Push {
    vec4 boundsPx;   // x, y, w, h in pixels, origin = top-left
    vec4 color;      // rgba
    vec4 uvRect;     
    vec2 viewportPx; // swapchain extent (width, height)
} push;

layout(location = 0) out vec4 fragColor;

void main()
{
    // 6-vertex quad from gl_VertexIndex
    const vec2 corners[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

    vec2 corner = corners[gl_VertexIndex];

    // Pixel-space vertex position
    vec2 posPx = push.boundsPx.xy + corner * push.boundsPx.zw;

    // Pixel -> NDC
    vec2 ndc;
    ndc.x = (posPx.x / push.viewportPx.x) * 2.0 - 1.0;
    ndc.y = (posPx.y / push.viewportPx.y) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    fragColor = push.color;
}
