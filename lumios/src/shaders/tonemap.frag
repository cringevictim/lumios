#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrInput;
layout(set = 0, binding = 1) uniform sampler2D bloomInput;

layout(push_constant) uniform PC {
    float exposure;
    float bloom_strength;
} params;

vec3 aces_tonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrInput, fragUV).rgb;
    vec3 bloom = texture(bloomInput, fragUV).rgb;

    hdr += bloom * params.bloom_strength;
    hdr *= params.exposure;

    vec3 mapped = aces_tonemap(hdr);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    outColor = vec4(mapped, 1.0);
}
