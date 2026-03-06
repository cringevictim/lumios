#version 450

layout(set = 0, binding = 0) uniform SkyboxUBO {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragTexCoord;

void main() {
    fragTexCoord = inPosition;
    mat4 rotView = mat4(mat3(ubo.view)); // remove translation
    vec4 clipPos = ubo.projection * rotView * vec4(inPosition, 1.0);
    gl_Position = clipPos.xyww; // max depth
}
