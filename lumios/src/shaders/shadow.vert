#version 450

layout(set = 0, binding = 0) uniform ShadowUBO {
    mat4 light_space_matrix;
} shadow;

layout(push_constant) uniform PC {
    mat4 model;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

void main() {
    gl_Position = shadow.light_space_matrix * model * vec4(inPosition, 1.0);
}
