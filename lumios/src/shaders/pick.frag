#version 450

layout(push_constant) uniform PC {
    mat4 model;
    uint entity_id;
};

layout(location = 0) out vec4 outColor;

void main() {
    float r = float((entity_id >>  0u) & 0xFFu) / 255.0;
    float g = float((entity_id >>  8u) & 0xFFu) / 255.0;
    float b = float((entity_id >> 16u) & 0xFFu) / 255.0;
    outColor = vec4(r, g, b, 1.0);
}
