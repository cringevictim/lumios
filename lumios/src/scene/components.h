#pragma once

#include "../math/math.h"
#include "../core/types.h"
#include "../graphics/gpu_types.h"
#include "../physics/physics_components.h"

namespace lumios {

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in degrees
    glm::vec3 scale{1.0f};

    glm::mat4 matrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = glm::rotate(m, glm::radians(rotation.y), {0, 1, 0});
        m = glm::rotate(m, glm::radians(rotation.x), {1, 0, 0});
        m = glm::rotate(m, glm::radians(rotation.z), {0, 0, 1});
        m = glm::scale(m, scale);
        return m;
    }
};

struct MeshComponent {
    MeshHandle     mesh;
    MaterialHandle material;
};

struct LightComponent {
    LightType type      = LightType::Point;
    glm::vec3 color     = {1.0f, 1.0f, 1.0f};
    float     intensity = 1.0f;
    float     range     = 20.0f;
    float     spot_angle = 45.0f;
};

struct NameComponent {
    std::string name;
};

struct CameraComponent {
    float fov        = 60.0f;
    float near_plane = 0.1f;
    float far_plane  = 1000.0f;
    bool  primary    = false;
};

struct ScriptComponent {
    std::string script_class;
};

struct ParticleEmitterComponent {
    u32       max_particles = 1000;
    float     emit_rate     = 100.0f;
    float     lifetime      = 2.0f;
    glm::vec3 velocity_min{-1.0f, 0.5f, -1.0f};
    glm::vec3 velocity_max{ 1.0f, 3.0f,  1.0f};
    glm::vec4 color_start{1.0f, 0.8f, 0.3f, 1.0f};
    glm::vec4 color_end{1.0f, 0.2f, 0.0f, 0.0f};
    float     size_start = 0.1f;
    float     size_end   = 0.0f;
    glm::vec3 gravity{0.0f, -9.8f, 0.0f};
};

} // namespace lumios
