#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 projection;
    vec4 camera_pos;
    vec4 ambient_color;
    int  num_lights;
} global;

struct Light {
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 params;     // x=range, y=spot_cos, z=type
};

layout(set = 0, binding = 1) uniform LightUBO {
    Light lights[16];
} lighting;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4  base_color;
    float metallic;
    float roughness;
    float ao;
} material;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

vec3 fresnel_schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(global.camera_pos.xyz - fragWorldPos);

    vec4 albedo_raw = texture(albedoMap, fragUV) * material.base_color * fragColor;
    vec3 albedo = albedo_raw.rgb;
    float metallic  = material.metallic;
    float roughness = max(material.roughness, 0.04);
    float ao        = material.ao;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < global.num_lights && i < 16; i++) {
        Light light = lighting.lights[i];
        int   type  = int(light.params.z);
        float intensity = light.color.a;

        vec3  L;
        float atten = 1.0;

        if (type == 0) {
            L = normalize(-light.direction.xyz);
        } else {
            vec3  toLight = light.position.xyz - fragWorldPos;
            float dist    = length(toLight);
            L = toLight / dist;
            float range = light.params.x;
            atten = clamp(1.0 - (dist * dist) / (range * range), 0.0, 1.0);
            atten *= atten;

            if (type == 2) {
                float cosA = dot(L, normalize(-light.direction.xyz));
                float cosOuter = light.params.y;
                atten *= clamp((cosA - cosOuter) / (1.0 - cosOuter), 0.0, 1.0);
            }
        }

        vec3 H = normalize(V + L);
        vec3 radiance = light.color.rgb * intensity * atten;

        float NDF = distribution_ggx(N, H, roughness);
        float G   = geometry_smith(N, V, L, roughness);
        vec3  F   = fresnel_schlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = global.ambient_color.rgb * global.ambient_color.a * albedo * ao;
    vec3 color   = ambient + Lo;

    outColor = vec4(color, albedo_raw.a);
}
