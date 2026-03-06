#include "physics_world.h"
#include "../core/log.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace lumios {

bool PhysicsWorld::init() {
    initialized_ = true;
    LOG_INFO("Physics world initialized (multi-shape solver + spatial hash)");
    return true;
}

void PhysicsWorld::shutdown() {
    bodies_.clear();
    grid_.clear();
    prev_contacts_.clear();
    curr_contacts_.clear();
    initialized_ = false;
}

void PhysicsWorld::sync_from_scene(Scene& scene) {
    bodies_.clear();
    auto view = scene.view<Transform, RigidbodyComponent>();
    for (auto entity : view) {
        auto& t  = view.get<Transform>(entity);
        auto& rb = view.get<RigidbodyComponent>(entity);

        BodyData bd{};
        bd.entity           = entity;
        bd.position         = t.position;
        bd.rotation         = t.rotation;
        bd.velocity         = glm::vec3(0.0f);
        bd.angular_velocity = glm::vec3(0.0f);
        bd.mass             = rb.mass;
        bd.linear_damping   = rb.linear_damping;
        bd.angular_damping  = rb.angular_damping;
        bd.use_gravity      = rb.use_gravity;
        bd.is_static        = (rb.type == RigidbodyComponent::Type::Static);
        bd.is_kinematic     = (rb.type == RigidbodyComponent::Type::Kinematic);

        if (scene.has<ColliderComponent>(entity)) {
            auto& col = scene.get<ColliderComponent>(entity);
            bd.shape        = col.shape;
            bd.half_extents = col.size * 0.5f;
            bd.radius       = col.radius;
            bd.height       = col.height;
            bd.offset       = col.offset;
            bd.restitution  = col.restitution;
            bd.friction     = col.friction;
            bd.is_trigger   = col.is_trigger;
            if (!col.hull_vertices.empty()) bd.hull_verts = &col.hull_vertices;
            if (!col.mesh_vertices.empty()) bd.mesh_verts = &col.mesh_vertices;
            if (!col.mesh_indices.empty())  bd.mesh_idx   = &col.mesh_indices;
        } else {
            bd.shape        = ColliderComponent::Shape::Box;
            bd.half_extents = t.scale * 0.5f;
            bd.radius       = std::max({t.scale.x, t.scale.y, t.scale.z}) * 0.5f;
            bd.height       = t.scale.y;
            bd.offset       = glm::vec3(0.0f);
            bd.restitution  = 0.3f;
            bd.friction     = 0.5f;
            bd.is_trigger   = false;
        }

        bodies_.push_back(bd);
    }
}

void PhysicsWorld::step(float dt) {
    if (!initialized_) return;

    accumulator_ += dt;
    while (accumulator_ >= fixed_timestep_) {
        for (auto& body : bodies_) {
            if (!body.is_static && !body.is_kinematic)
                integrate(body, fixed_timestep_);
        }
        build_spatial_grid();
        resolve_collisions();
        accumulator_ -= fixed_timestep_;
    }
}

void PhysicsWorld::integrate(BodyData& body, float dt) {
    if (body.use_gravity)
        body.velocity += gravity_ * dt;

    body.velocity *= (1.0f - body.linear_damping * dt);
    body.angular_velocity *= (1.0f - body.angular_damping * dt);

    body.position += body.velocity * dt;
    body.rotation += body.angular_velocity * dt;
}

// --- Spatial hash grid ---

glm::vec3 PhysicsWorld::get_aabb_min(const BodyData& b) const {
    glm::vec3 center = b.position + b.offset;
    float r = get_bounding_radius(b);
    return center - glm::vec3(r);
}

glm::vec3 PhysicsWorld::get_aabb_max(const BodyData& b) const {
    glm::vec3 center = b.position + b.offset;
    float r = get_bounding_radius(b);
    return center + glm::vec3(r);
}

float PhysicsWorld::get_bounding_radius(const BodyData& b) const {
    switch (b.shape) {
        case ColliderComponent::Shape::Sphere:
            return b.radius;
        case ColliderComponent::Shape::Capsule:
            return glm::max(b.radius, b.height * 0.5f + b.radius);
        case ColliderComponent::Shape::Box:
        default:
            return glm::length(b.half_extents);
    }
}

void PhysicsWorld::build_spatial_grid() {
    grid_.clear();
    for (u32 i = 0; i < static_cast<u32>(bodies_.size()); i++) {
        glm::vec3 mn = get_aabb_min(bodies_[i]);
        glm::vec3 mx = get_aabb_max(bodies_[i]);
        i32 x0 = static_cast<i32>(std::floor(mn.x / cell_size_));
        i32 y0 = static_cast<i32>(std::floor(mn.y / cell_size_));
        i32 z0 = static_cast<i32>(std::floor(mn.z / cell_size_));
        i32 x1 = static_cast<i32>(std::floor(mx.x / cell_size_));
        i32 y1 = static_cast<i32>(std::floor(mx.y / cell_size_));
        i32 z1 = static_cast<i32>(std::floor(mx.z / cell_size_));
        for (i32 x = x0; x <= x1; x++)
            for (i32 y = y0; y <= y1; y++)
                for (i32 z = z0; z <= z1; z++)
                    grid_[{x, y, z}].push_back(i);
    }
}

// --- Collision tests ---

PhysicsWorld::CollisionResult PhysicsWorld::test_box_box(const BodyData& a, const BodyData& b) {
    CollisionResult r;
    glm::vec3 ac = a.position + a.offset, bc = b.position + b.offset;
    glm::vec3 a_min = ac - a.half_extents, a_max = ac + a.half_extents;
    glm::vec3 b_min = bc - b.half_extents, b_max = bc + b.half_extents;

    if (a_min.x > b_max.x || a_max.x < b_min.x) return r;
    if (a_min.y > b_max.y || a_max.y < b_min.y) return r;
    if (a_min.z > b_max.z || a_max.z < b_min.z) return r;

    r.hit = true;
    float ox = std::min(a_max.x, b_max.x) - std::max(a_min.x, b_min.x);
    float oy = std::min(a_max.y, b_max.y) - std::max(a_min.y, b_min.y);
    float oz = std::min(a_max.z, b_max.z) - std::max(a_min.z, b_min.z);

    if (ox < oy && ox < oz) {
        r.penetration = ox;
        r.normal = (ac.x < bc.x) ? glm::vec3(-1, 0, 0) : glm::vec3(1, 0, 0);
    } else if (oy < oz) {
        r.penetration = oy;
        r.normal = (ac.y < bc.y) ? glm::vec3(0, -1, 0) : glm::vec3(0, 1, 0);
    } else {
        r.penetration = oz;
        r.normal = (ac.z < bc.z) ? glm::vec3(0, 0, -1) : glm::vec3(0, 0, 1);
    }
    r.contact = (ac + bc) * 0.5f;
    return r;
}

PhysicsWorld::CollisionResult PhysicsWorld::test_sphere_sphere(const BodyData& a, const BodyData& b) {
    CollisionResult r;
    glm::vec3 ac = a.position + a.offset, bc = b.position + b.offset;
    glm::vec3 d = bc - ac;
    float dist2 = glm::dot(d, d);
    float sum_r = a.radius + b.radius;
    if (dist2 > sum_r * sum_r) return r;

    float dist = std::sqrt(dist2);
    r.hit = true;
    r.normal = (dist > 0.0001f) ? d / dist : glm::vec3(0, 1, 0);
    r.penetration = sum_r - dist;
    r.contact = ac + r.normal * a.radius;
    return r;
}

PhysicsWorld::CollisionResult PhysicsWorld::test_sphere_box(const BodyData& sphere, const BodyData& box) {
    CollisionResult r;
    glm::vec3 sc = sphere.position + sphere.offset;
    glm::vec3 bc = box.position + box.offset;

    glm::vec3 closest = glm::clamp(sc, bc - box.half_extents, bc + box.half_extents);
    glm::vec3 d = sc - closest;
    float dist2 = glm::dot(d, d);

    if (dist2 > sphere.radius * sphere.radius) return r;

    r.hit = true;
    float dist = std::sqrt(dist2);
    if (dist > 0.0001f) {
        r.normal = d / dist;
        r.penetration = sphere.radius - dist;
    } else {
        glm::vec3 diff = sc - bc;
        glm::vec3 abs_diff = glm::abs(diff);
        glm::vec3 overlap = box.half_extents - abs_diff;
        if (overlap.x < overlap.y && overlap.x < overlap.z)
            r.normal = glm::vec3(diff.x >= 0 ? 1 : -1, 0, 0);
        else if (overlap.y < overlap.z)
            r.normal = glm::vec3(0, diff.y >= 0 ? 1 : -1, 0);
        else
            r.normal = glm::vec3(0, 0, diff.z >= 0 ? 1 : -1);
        r.penetration = sphere.radius + std::min({overlap.x, overlap.y, overlap.z});
    }
    r.contact = closest;
    return r;
}

static glm::vec3 closest_point_on_segment(const glm::vec3& a, const glm::vec3& b, const glm::vec3& p) {
    glm::vec3 ab = b - a;
    float t = glm::dot(p - a, ab) / glm::max(glm::dot(ab, ab), 0.0001f);
    return a + glm::clamp(t, 0.0f, 1.0f) * ab;
}

static void closest_points_segments(const glm::vec3& a0, const glm::vec3& a1,
                                     const glm::vec3& b0, const glm::vec3& b1,
                                     glm::vec3& out_a, glm::vec3& out_b) {
    glm::vec3 d1 = a1 - a0, d2 = b1 - b0, r = a0 - b0;
    float a = glm::dot(d1, d1), e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);

    float s, t;
    if (a < 0.0001f && e < 0.0001f) { s = t = 0.0f; }
    else if (a < 0.0001f) { s = 0.0f; t = glm::clamp(f / e, 0.0f, 1.0f); }
    else {
        float c = glm::dot(d1, r);
        if (e < 0.0001f) { t = 0.0f; s = glm::clamp(-c / a, 0.0f, 1.0f); }
        else {
            float b_val = glm::dot(d1, d2);
            float denom = a * e - b_val * b_val;
            s = (denom != 0.0f) ? glm::clamp((b_val * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b_val * s + f) / e;
            if (t < 0.0f) { t = 0.0f; s = glm::clamp(-c / a, 0.0f, 1.0f); }
            else if (t > 1.0f) { t = 1.0f; s = glm::clamp((b_val - c) / a, 0.0f, 1.0f); }
        }
    }
    out_a = a0 + d1 * s;
    out_b = b0 + d2 * t;
}

PhysicsWorld::CollisionResult PhysicsWorld::test_capsule_capsule(const BodyData& a, const BodyData& b) {
    CollisionResult r;
    float ah = a.height * 0.5f, bh = b.height * 0.5f;
    glm::vec3 ac = a.position + a.offset, bc = b.position + b.offset;
    glm::vec3 a0 = ac + glm::vec3(0, -ah, 0), a1 = ac + glm::vec3(0, ah, 0);
    glm::vec3 b0 = bc + glm::vec3(0, -bh, 0), b1 = bc + glm::vec3(0, bh, 0);

    glm::vec3 pa, pb;
    closest_points_segments(a0, a1, b0, b1, pa, pb);

    glm::vec3 d = pb - pa;
    float dist2 = glm::dot(d, d);
    float sum_r = a.radius + b.radius;
    if (dist2 > sum_r * sum_r) return r;

    float dist = std::sqrt(dist2);
    r.hit = true;
    r.normal = (dist > 0.0001f) ? d / dist : glm::vec3(0, 1, 0);
    r.penetration = sum_r - dist;
    r.contact = pa + r.normal * a.radius;
    return r;
}

PhysicsWorld::CollisionResult PhysicsWorld::test_capsule_sphere(const BodyData& capsule, const BodyData& sphere) {
    CollisionResult r;
    float ch = capsule.height * 0.5f;
    glm::vec3 cc = capsule.position + capsule.offset;
    glm::vec3 c0 = cc + glm::vec3(0, -ch, 0), c1 = cc + glm::vec3(0, ch, 0);
    glm::vec3 sc = sphere.position + sphere.offset;

    glm::vec3 closest = closest_point_on_segment(c0, c1, sc);
    glm::vec3 d = sc - closest;
    float dist2 = glm::dot(d, d);
    float sum_r = capsule.radius + sphere.radius;
    if (dist2 > sum_r * sum_r) return r;

    float dist = std::sqrt(dist2);
    r.hit = true;
    r.normal = (dist > 0.0001f) ? d / dist : glm::vec3(0, 1, 0);
    r.penetration = sum_r - dist;
    r.contact = closest + r.normal * capsule.radius;
    return r;
}

PhysicsWorld::CollisionResult PhysicsWorld::test_capsule_box(const BodyData& capsule, const BodyData& box) {
    CollisionResult r;
    float ch = capsule.height * 0.5f;
    glm::vec3 cc = capsule.position + capsule.offset;
    glm::vec3 bc = box.position + box.offset;
    glm::vec3 c0 = cc + glm::vec3(0, -ch, 0), c1 = cc + glm::vec3(0, ch, 0);

    glm::vec3 b_min = bc - box.half_extents, b_max = bc + box.half_extents;

    float best_dist2 = FLT_MAX;
    glm::vec3 best_cap_pt, best_box_pt;
    constexpr int SAMPLES = 8;
    for (int i = 0; i <= SAMPLES; i++) {
        float t = static_cast<float>(i) / SAMPLES;
        glm::vec3 pt = c0 + (c1 - c0) * t;
        glm::vec3 clamped = glm::clamp(pt, b_min, b_max);
        float d2 = glm::dot(pt - clamped, pt - clamped);
        if (d2 < best_dist2) {
            best_dist2 = d2;
            best_cap_pt = pt;
            best_box_pt = clamped;
        }
    }

    float dist = std::sqrt(best_dist2);
    if (dist > capsule.radius && best_dist2 > 0.0001f) return r;

    r.hit = true;
    if (best_dist2 > 0.0001f) {
        r.normal = (best_cap_pt - best_box_pt) / dist;
        r.penetration = capsule.radius - dist;
    } else {
        glm::vec3 diff = best_cap_pt - bc;
        glm::vec3 abs_diff = glm::abs(diff);
        glm::vec3 overlap = box.half_extents - abs_diff;
        if (overlap.x < overlap.y && overlap.x < overlap.z)
            r.normal = glm::vec3(diff.x >= 0 ? 1 : -1, 0, 0);
        else if (overlap.y < overlap.z)
            r.normal = glm::vec3(0, diff.y >= 0 ? 1 : -1, 0);
        else
            r.normal = glm::vec3(0, 0, diff.z >= 0 ? 1 : -1);
        r.penetration = capsule.radius + std::min({overlap.x, overlap.y, overlap.z});
    }
    r.contact = best_box_pt;
    return r;
}

// GJK support function for convex hull
static glm::vec3 support_hull(const std::vector<glm::vec3>& verts, const glm::vec3& pos, const glm::vec3& dir) {
    float best = -FLT_MAX;
    glm::vec3 result = pos;
    for (auto& v : verts) {
        glm::vec3 world = v + pos;
        float d = glm::dot(world, dir);
        if (d > best) { best = d; result = world; }
    }
    return result;
}

static glm::vec3 support_body(const PhysicsWorld::BodyData& b, const glm::vec3& dir) {
    glm::vec3 c = b.position + b.offset;
    using S = ColliderComponent::Shape;
    switch (b.shape) {
        case S::Sphere:
            return c + glm::normalize(dir) * b.radius;
        case S::Box:
            return c + glm::vec3(
                dir.x >= 0 ? b.half_extents.x : -b.half_extents.x,
                dir.y >= 0 ? b.half_extents.y : -b.half_extents.y,
                dir.z >= 0 ? b.half_extents.z : -b.half_extents.z);
        case S::Capsule: {
            float ch = b.height * 0.5f;
            glm::vec3 top = c + glm::vec3(0, ch, 0);
            glm::vec3 bot = c + glm::vec3(0, -ch, 0);
            glm::vec3 base = (glm::dot(top, dir) > glm::dot(bot, dir)) ? top : bot;
            return base + glm::normalize(dir) * b.radius;
        }
        case S::ConvexHull:
            if (b.hull_verts && !b.hull_verts->empty())
                return support_hull(*b.hull_verts, c, dir);
            return c + glm::vec3(
                dir.x >= 0 ? b.half_extents.x : -b.half_extents.x,
                dir.y >= 0 ? b.half_extents.y : -b.half_extents.y,
                dir.z >= 0 ? b.half_extents.z : -b.half_extents.z);
        default:
            return c;
    }
}

PhysicsWorld::CollisionResult PhysicsWorld::test_convex_convex(const BodyData& a, const BodyData& b) {
    CollisionResult r;

    // Simplified GJK - check overlap via SAT on center-to-center axis + 6 principal axes
    glm::vec3 ac = a.position + a.offset, bc = b.position + b.offset;
    glm::vec3 axes[] = {
        glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1),
        glm::normalize(bc - ac + glm::vec3(0.0001f))
    };

    float min_pen = FLT_MAX;
    glm::vec3 min_axis;

    for (auto& axis : axes) {
        if (glm::length(axis) < 0.0001f) continue;
        glm::vec3 n = glm::normalize(axis);

        float a_min = FLT_MAX, a_max = -FLT_MAX;
        float b_min = FLT_MAX, b_max = -FLT_MAX;

        auto project = [&](const PhysicsWorld::BodyData& body, float& lo, float& hi) {
            for (int sign = -1; sign <= 1; sign += 2) {
                glm::vec3 pt = support_body(body, n * static_cast<float>(sign));
                float d = glm::dot(pt, n);
                lo = std::min(lo, d);
                hi = std::max(hi, d);
            }
        };
        project(a, a_min, a_max);
        project(b, b_min, b_max);

        float overlap = std::min(a_max, b_max) - std::max(a_min, b_min);
        if (overlap <= 0.0f) return r;

        if (overlap < min_pen) {
            min_pen = overlap;
            min_axis = (glm::dot(bc - ac, n) < 0) ? -n : n;
        }
    }

    r.hit = true;
    r.normal = -min_axis;
    r.penetration = min_pen;
    r.contact = (ac + bc) * 0.5f;
    return r;
}

PhysicsWorld::CollisionResult PhysicsWorld::test_pair(const BodyData& a, const BodyData& b) {
    using S = ColliderComponent::Shape;

    bool a_convex = (a.shape == S::ConvexHull || a.shape == S::Mesh);
    bool b_convex = (b.shape == S::ConvexHull || b.shape == S::Mesh);

    if (a_convex || b_convex) return test_convex_convex(a, b);

    if (a.shape == S::Box && b.shape == S::Box) return test_box_box(a, b);
    if (a.shape == S::Sphere && b.shape == S::Sphere) return test_sphere_sphere(a, b);

    if (a.shape == S::Sphere && b.shape == S::Box) return test_sphere_box(a, b);
    if (a.shape == S::Box && b.shape == S::Sphere) {
        auto r = test_sphere_box(b, a);
        r.normal = -r.normal;
        return r;
    }

    if (a.shape == S::Capsule && b.shape == S::Capsule) return test_capsule_capsule(a, b);

    if (a.shape == S::Capsule && b.shape == S::Sphere) return test_capsule_sphere(a, b);
    if (a.shape == S::Sphere && b.shape == S::Capsule) {
        auto r = test_capsule_sphere(b, a);
        r.normal = -r.normal;
        return r;
    }

    if (a.shape == S::Capsule && b.shape == S::Box) return test_capsule_box(a, b);
    if (a.shape == S::Box && b.shape == S::Capsule) {
        auto r = test_capsule_box(b, a);
        r.normal = -r.normal;
        return r;
    }

    return test_box_box(a, b);
}

void PhysicsWorld::resolve_impulse(BodyData& a, BodyData& b, const CollisionResult& cr) {
    if (a.is_static && b.is_static) return;

    float inv_a = a.is_static ? 0.0f : 1.0f / std::max(a.mass, 0.001f);
    float inv_b = b.is_static ? 0.0f : 1.0f / std::max(b.mass, 0.001f);
    float total = inv_a + inv_b;
    if (total < 0.0001f) return;

    glm::vec3 correction = cr.normal * (cr.penetration / total) * 0.8f;
    if (!a.is_static) a.position += correction * inv_a;
    if (!b.is_static) b.position -= correction * inv_b;

    glm::vec3 rel_vel = b.velocity - a.velocity;
    float vel_n = glm::dot(rel_vel, cr.normal);
    if (vel_n > 0) return;

    float e = std::min(a.restitution, b.restitution);
    float j = -(1.0f + e) * vel_n / total;

    glm::vec3 impulse = cr.normal * j;
    if (!a.is_static) a.velocity -= impulse * inv_a;
    if (!b.is_static) b.velocity += impulse * inv_b;

    // Friction
    glm::vec3 tangent = rel_vel - cr.normal * vel_n;
    float tang_len = glm::length(tangent);
    if (tang_len > 0.0001f) {
        tangent /= tang_len;
        float jt = -glm::dot(rel_vel, tangent) / total;
        float mu = std::sqrt(a.friction * b.friction);
        jt = glm::clamp(jt, -j * mu, j * mu);
        glm::vec3 friction_impulse = tangent * jt;
        if (!a.is_static) a.velocity -= friction_impulse * inv_a;
        if (!b.is_static) b.velocity += friction_impulse * inv_b;
    }
}

void PhysicsWorld::resolve_collisions() {
    frame_events_.clear();
    frame_triggers_.clear();
    curr_contacts_.clear();
    contact_infos_.clear();

    std::set<std::pair<u32, u32>> tested;

    for (auto& [cell, indices] : grid_) {
        for (size_t ii = 0; ii < indices.size(); ii++) {
            for (size_t jj = ii + 1; jj < indices.size(); jj++) {
                u32 i = indices[ii], j = indices[jj];
                if (i > j) std::swap(i, j);
                if (!tested.insert({i, j}).second) continue;

                auto& a = bodies_[i];
                auto& b = bodies_[j];
                if (a.is_static && b.is_static) continue;

                auto cr = test_pair(a, b);
                if (!cr.hit) continue;

                ContactPair cp{a.entity, b.entity};
                curr_contacts_.insert(cp);

                CollisionEvent ev;
                ev.a = a.entity;
                ev.b = b.entity;
                ev.contact_point = cr.contact;
                ev.normal = cr.normal;
                ev.penetration = cr.penetration;
                ev.is_trigger = a.is_trigger || b.is_trigger;

                if (ev.is_trigger) {
                    frame_triggers_.push_back(ev);
                } else {
                    resolve_impulse(a, b, cr);
                    frame_events_.push_back(ev);
                }
            }
        }
    }

    // Determine enter/stay/exit states
    for (auto& cp : curr_contacts_) {
        ContactState state = prev_contacts_.count(cp) ? ContactState::Stay : ContactState::Enter;
        CollisionEvent ev{};
        ev.a = cp.a; ev.b = cp.b;
        for (auto& fe : frame_events_)
            if ((fe.a == cp.a && fe.b == cp.b) || (fe.a == cp.b && fe.b == cp.a))
                { ev = fe; break; }
        for (auto& ft : frame_triggers_)
            if ((ft.a == cp.a && ft.b == cp.b) || (ft.a == cp.b && ft.b == cp.a))
                { ev = ft; break; }
        contact_infos_.push_back({cp, state, ev});
    }
    for (auto& cp : prev_contacts_) {
        if (!curr_contacts_.count(cp)) {
            CollisionEvent ev{}; ev.a = cp.a; ev.b = cp.b;
            contact_infos_.push_back({cp, ContactState::Exit, ev});
        }
    }

    prev_contacts_ = curr_contacts_;
}

void PhysicsWorld::sync_to_scene(Scene& scene) {
    for (auto& body : bodies_) {
        if (!scene.registry().valid(body.entity)) continue;
        if (body.is_static) continue;
        auto& t = scene.get<Transform>(body.entity);
        t.position = body.position;
        t.rotation = body.rotation;
    }
}

} // namespace lumios
