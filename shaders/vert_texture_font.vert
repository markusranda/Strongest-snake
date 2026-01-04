#version 450

layout(push_constant) uniform Push {
    vec4 boundsPx;   // x, y, w, h in pixels, origin = top-left
    vec4 color;      // rgba
    vec4 uvRect;     
    vec2 viewportPx; // swapchain extent (width, height)
    int triangle;
} push;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    vec2 corner = vec2(0.0, 0.0);
    if (push.triangle == 1) {
        const vec2 corners[3] = vec2[3](
            vec2(0.0, 0.0),
            vec2(1.0, 0.5),
            vec2(0.0, 1.0)
        );
        corner = corners[gl_VertexIndex];
    } else {
        const vec2 corners[6] = vec2[6](
            vec2(0.0, 0.0),
            vec2(1.0, 0.0),
            vec2(0.0, 1.0),
            vec2(0.0, 1.0),
            vec2(1.0, 0.0),
            vec2(1.0, 1.0)
        );
        corner = corners[gl_VertexIndex];
    }

    // Pixel position
    vec2 pixelPos = push.boundsPx.xy + corner * push.boundsPx.zw;

    // Pixel -> NDC (top-left origin)
    vec2 ndc;
    ndc.x = (pixelPos.x / push.viewportPx.x) * 2.0 - 1.0;
    ndc.y = (pixelPos.y / push.viewportPx.y) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);

    fragColor = push.color;
    fragUV = mix(push.uvRect.xy, push.uvRect.zw, corner);
}
