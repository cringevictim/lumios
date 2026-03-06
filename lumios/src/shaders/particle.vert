#version 450

struct Particle {
    vec4 position; // xyz = pos, w = life
    vec4 velocity; // xyz = vel, w = size
    vec4 color;
};

layout(std430, set = 0, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

layout(set = 0, binding = 1) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec3 camera_right;
    vec3 camera_up;
} cam;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

const vec2 quad_verts[6] = vec2[](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5),  vec2(-0.5, 0.5)
);

void main() {
    uint particle_idx = gl_VertexIndex / 6;
    uint vert_idx     = gl_VertexIndex % 6;

    Particle p = particles[particle_idx];
    if (p.position.w <= 0.0) {
        gl_Position = vec4(0.0);
        return;
    }

    vec2 offset = quad_verts[vert_idx];
    float size  = p.velocity.w;

    vec3 world_pos = p.position.xyz
        + cam.camera_right * offset.x * size
        + cam.camera_up    * offset.y * size;

    gl_Position = cam.projection * cam.view * vec4(world_pos, 1.0);
    fragColor = p.color;
    fragUV    = offset + 0.5;
}
