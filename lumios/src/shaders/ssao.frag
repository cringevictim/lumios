#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D depthTex;
layout(set = 0, binding = 1) uniform sampler2D noiseTex;

layout(set = 0, binding = 2) uniform SSAOParams {
    mat4 projection;
    mat4 view;
    vec4 samples[64];
    vec2 noise_scale;
    float radius;
    float bias;
    int kernel_size;
} params;

vec3 reconstruct_position(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view_pos = inverse(params.projection) * clip;
    return view_pos.xyz / view_pos.w;
}

void main() {
    float depth = texture(depthTex, fragUV).r;
    if (depth >= 1.0) { outAO = 1.0; return; }

    vec3 frag_pos = reconstruct_position(fragUV, depth);
    vec3 noise = texture(noiseTex, fragUV * params.noise_scale).xyz * 2.0 - 1.0;

    // Approximate normal from depth
    vec3 dFdxPos = dFdx(frag_pos);
    vec3 dFdyPos = dFdy(frag_pos);
    vec3 normal = normalize(cross(dFdxPos, dFdyPos));

    vec3 tangent   = normalize(noise - normal * dot(noise, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < params.kernel_size; i++) {
        vec3 sample_pos = TBN * params.samples[i].xyz;
        sample_pos = frag_pos + sample_pos * params.radius;

        vec4 offset = params.projection * vec4(sample_pos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        float sample_depth = texture(depthTex, offset.xy).r;
        vec3 sampled_pos = reconstruct_position(offset.xy, sample_depth);

        float range_check = smoothstep(0.0, 1.0, params.radius / abs(frag_pos.z - sampled_pos.z));
        occlusion += (sampled_pos.z >= sample_pos.z + params.bias ? 1.0 : 0.0) * range_check;
    }

    outAO = 1.0 - (occlusion / float(params.kernel_size));
}
