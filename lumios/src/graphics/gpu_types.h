#pragma once

#include "../core/types.h"
#include "../math/math.h"

namespace lumios {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color{1.0f};
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32>    indices;
};

enum class LightType : int { Directional = 0, Point = 1, Spot = 2 };

struct MaterialData {
    glm::vec4 base_color{1.0f};
    float metallic  = 0.0f;
    float roughness = 0.5f;
    float ao        = 1.0f;
    TextureHandle albedo_texture{};
};

// GPU-side uniform structs (std140 alignment)

struct alignas(16) GlobalUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 camera_pos;
    glm::vec4 ambient_color;
    int       num_lights;
    int       _pad[3];
};

struct alignas(16) GPULight {
    glm::vec4 position;   // w=0 directional, w=1 point/spot
    glm::vec4 color;      // rgb + intensity in w
    glm::vec4 direction;  // normalized direction
    glm::vec4 params;     // x=range, y=spot_angle_cos, z=type, w=unused
};

struct alignas(16) LightUBO {
    GPULight lights[16];
};

struct alignas(16) MaterialUBOData {
    glm::vec4 base_color;
    float metallic;
    float roughness;
    float ao;
    float _pad;
};

struct PushConstants {
    glm::mat4 model;
};

} // namespace lumios
