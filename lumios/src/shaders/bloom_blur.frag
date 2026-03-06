#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTex;

layout(push_constant) uniform PC {
    float horizontal; // 1.0 = horizontal, 0.0 = vertical
} params;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 tex_offset = 1.0 / textureSize(inputTex, 0);
    vec3 result = texture(inputTex, fragUV).rgb * weights[0];

    if (params.horizontal > 0.5) {
        for (int i = 1; i < 5; i++) {
            result += texture(inputTex, fragUV + vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
            result += texture(inputTex, fragUV - vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
        }
    } else {
        for (int i = 1; i < 5; i++) {
            result += texture(inputTex, fragUV + vec2(0.0, tex_offset.y * i)).rgb * weights[i];
            result += texture(inputTex, fragUV - vec2(0.0, tex_offset.y * i)).rgb * weights[i];
        }
    }

    outColor = vec4(result, 1.0);
}
