#pragma once

#include "../math/math.h"
#include "../core/types.h"
#include <entt/entt.hpp>
#include <vector>

namespace lumios {

struct RigidbodyComponent {
    enum class Type : int { Static = 0, Dynamic = 1, Kinematic = 2 };
    Type  type            = Type::Dynamic;
    float mass            = 1.0f;
    float linear_damping  = 0.05f;
    float angular_damping = 0.05f;
    bool  use_gravity     = true;
    u32   body_id         = UINT32_MAX;
};

struct ColliderComponent {
    enum class Shape : int { Box = 0, Sphere = 1, Capsule = 2, Mesh = 3, ConvexHull = 4 };
    Shape     shape       = Shape::Box;
    glm::vec3 size{1.0f};
    glm::vec3 offset{0.0f};
    float     radius      = 0.5f;
    float     height      = 1.0f;
    float     friction    = 0.5f;
    float     restitution = 0.3f;
    bool      is_trigger  = false;
    float     hull_detail = 0.5f;
    std::vector<glm::vec3> hull_vertices;
    std::vector<glm::vec3> mesh_vertices;
    std::vector<u32>       mesh_indices;
};

struct CollisionEvent {
    entt::entity a, b;
    glm::vec3 contact_point{0.0f};
    glm::vec3 normal{0.0f};
    float penetration = 0.0f;
    bool is_trigger = false;
};

struct CharacterControllerComponent {
    float move_speed         = 5.0f;
    float sprint_multiplier  = 2.0f;
    float jump_force         = 5.0f;
    float mouse_sensitivity  = 0.15f;
    float gravity_multiplier = 1.0f;
    bool  is_grounded        = false;
    glm::vec3 velocity{0.0f};
};

} // namespace lumios
