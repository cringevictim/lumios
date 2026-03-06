#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 projection;
    vec4 camera_pos;
    vec4 ambient_color;
    int  num_lights;
} global;

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragColor;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position   = global.projection * global.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragNormal   = mat3(transpose(inverse(push.model))) * inNormal;
    fragUV       = inUV;
    fragColor    = inColor;
}
