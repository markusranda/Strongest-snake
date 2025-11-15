#version 450

layout(push_constant) uniform Push {
    mat4 viewProj;
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
    if (p.alive == 0) {
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0); // outside clip space
        gl_PointSize = 0.0;                     // <- REQUIRED
        return;
    }

    gl_Position = push.viewProj * vec4(p.pos, 0.0, 1.0);
    gl_PointSize = 5.0;  // small dot size
}
