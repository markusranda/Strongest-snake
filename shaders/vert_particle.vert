#version 450

layout(push_constant) uniform Push {
    mat4 viewProj;
    float zoom;
} push;

// IMPORTANT These structs need to match the insides of ParticleSystem.h
struct Particle {
    vec2 pos;
    vec2 vel;
    float life;
    uint alive;
};

layout(std430, binding = 0) readonly buffer Particles {
    Particle particles[];
};

void main() {
    uint id = gl_VertexIndex;
    Particle p = particles[id];
    gl_Position = push.viewProj * vec4(p.pos, 0.0, 1.0);
    float pixelSize = 5.0;
    gl_PointSize = pixelSize * push.zoom;
}
