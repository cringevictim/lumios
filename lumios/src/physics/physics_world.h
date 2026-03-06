#pragma once

#include "physics_components.h"
#include "../scene/scene.h"
#include "../scene/components.h"
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace lumios {

class PhysicsWorld {
public:
    bool init();
    void shutdown();

    void sync_from_scene(Scene& scene);
    void step(float dt);
    void sync_to_scene(Scene& scene);

    void set_gravity(const glm::vec3& g) { gravity_ = g; }
    void set_fixed_timestep(float ts) { fixed_timestep_ = ts; }

    const std::vector<CollisionEvent>& collision_events() const { return frame_events_; }
    const std::vector<CollisionEvent>& trigger_events() const { return frame_triggers_; }

    struct ContactPair {
        entt::entity a, b;
        bool operator==(const ContactPair& o) const {
            return (a == o.a && b == o.b) || (a == o.b && b == o.a);
        }
    };
    struct ContactPairHash {
        size_t operator()(const ContactPair& p) const {
            auto h1 = std::hash<u32>{}(static_cast<u32>(p.a));
            auto h2 = std::hash<u32>{}(static_cast<u32>(p.b));
            return h1 ^ (h2 << 16) ^ (h2 >> 16);
        }
    };

    enum class ContactState { Enter, Stay, Exit };
    struct ContactInfo {
        ContactPair pair;
        ContactState state;
        CollisionEvent event;
    };
    const std::vector<ContactInfo>& contact_infos() const { return contact_infos_; }

    struct BodyData {
        entt::entity entity;
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 rotation;
        glm::vec3 angular_velocity;
        float     mass;
        float     linear_damping;
        float     angular_damping;
        bool      use_gravity;
        bool      is_static;
        bool      is_kinematic;
        bool      is_trigger;

        ColliderComponent::Shape shape;
        glm::vec3 half_extents;
        float     radius;
        float     height;
        glm::vec3 offset;
        float     restitution;
        float     friction;

        const std::vector<glm::vec3>* hull_verts = nullptr;
        const std::vector<glm::vec3>* mesh_verts = nullptr;
        const std::vector<u32>*       mesh_idx   = nullptr;
    };

private:
    glm::vec3 gravity_{0.0f, -9.81f, 0.0f};
    float accumulator_    = 0.0f;
    float fixed_timestep_ = 1.0f / 60.0f;
    bool  initialized_    = false;

    std::vector<BodyData> bodies_;

    // Collision events for this frame
    std::vector<CollisionEvent> frame_events_;
    std::vector<CollisionEvent> frame_triggers_;
    std::vector<ContactInfo>    contact_infos_;

    std::unordered_set<ContactPair, ContactPairHash> prev_contacts_;
    std::unordered_set<ContactPair, ContactPairHash> curr_contacts_;

    // Spatial hash grid
    struct CellKey {
        i32 x, y, z;
        bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    struct CellKeyHash {
        size_t operator()(const CellKey& k) const {
            size_t h = std::hash<i32>{}(k.x);
            h ^= std::hash<i32>{}(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<i32>{}(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    float cell_size_ = 4.0f;
    std::unordered_map<CellKey, std::vector<u32>, CellKeyHash> grid_;

    void integrate(BodyData& body, float dt);
    void build_spatial_grid();
    void resolve_collisions();

    struct CollisionResult {
        bool hit = false;
        glm::vec3 normal{0.0f};
        float penetration = 0.0f;
        glm::vec3 contact{0.0f};
    };

    CollisionResult test_pair(const BodyData& a, const BodyData& b);
    CollisionResult test_box_box(const BodyData& a, const BodyData& b);
    CollisionResult test_sphere_sphere(const BodyData& a, const BodyData& b);
    CollisionResult test_sphere_box(const BodyData& sphere, const BodyData& box);
    CollisionResult test_capsule_capsule(const BodyData& a, const BodyData& b);
    CollisionResult test_capsule_sphere(const BodyData& capsule, const BodyData& sphere);
    CollisionResult test_capsule_box(const BodyData& capsule, const BodyData& box);
    CollisionResult test_convex_convex(const BodyData& a, const BodyData& b);

    void resolve_impulse(BodyData& a, BodyData& b, const CollisionResult& cr);

    glm::vec3 get_aabb_min(const BodyData& b) const;
    glm::vec3 get_aabb_max(const BodyData& b) const;
    float get_bounding_radius(const BodyData& b) const;
};

} // namespace lumios
