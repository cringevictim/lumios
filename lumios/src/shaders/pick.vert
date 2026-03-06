#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 projection;
    vec4 camera_pos;
    vec4 ambient_color;
    int  num_lights;
} global;

layout(push_constant) uniform PC {
    mat4 model;
    uint entity_id;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

void main() {
    gl_Position = global.projection * global.view * model * vec4(inPosition, 1.0);
}
