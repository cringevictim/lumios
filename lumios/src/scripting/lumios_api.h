#pragma once

#include "../scene/scene.h"
#include "../scene/components.h"
#include "../core/input.h"
#include "../core/log.h"
#include "../math/math.h"
#include <string>
#include <vector>
#include <functional>

namespace lumios {

// --- Collision info passed to script callbacks ---

struct CollisionInfo {
    entt::entity other;
    glm::vec3    contact_point{0.0f};
    glm::vec3    normal{0.0f};
    float        penetration = 0.0f;
};

// --- Property system for exposing script fields to the editor ---

enum class PropertyType { Float, Int, Bool, Vec3, String };

struct PropertyInfo {
    const char*  name;
    PropertyType type;
    size_t       offset;
    float        min_val = 0.0f;
    float        max_val = 0.0f;
};

#define LUMIOS_PROPERTIES_BEGIN(ClassName) \
    public: static std::vector<lumios::PropertyInfo> lumios_get_properties() { \
        using _Self = ClassName; \
        std::vector<lumios::PropertyInfo> props;

#define LUMIOS_PROP_FLOAT(name, mn, mx) \
    props.push_back({#name, lumios::PropertyType::Float, offsetof(_Self, name), mn, mx});

#define LUMIOS_PROP_INT(name, mn, mx) \
    props.push_back({#name, lumios::PropertyType::Int, offsetof(_Self, name), static_cast<float>(mn), static_cast<float>(mx)});

#define LUMIOS_PROP_BOOL(name) \
    props.push_back({#name, lumios::PropertyType::Bool, offsetof(_Self, name)});

#define LUMIOS_PROP_VEC3(name) \
    props.push_back({#name, lumios::PropertyType::Vec3, offsetof(_Self, name)});

#define LUMIOS_PROP_STRING(name) \
    props.push_back({#name, lumios::PropertyType::String, offsetof(_Self, name)});

#define LUMIOS_PROPERTIES_END() \
        return props; \
    }

// --- Script context: the main interface scripts use to interact with the engine ---

struct ScriptContext {
    Scene&       scene;
    entt::entity entity;
    float        delta_time;
    Input*       input;

    // --- Shorthand ---
    float dt() const { return delta_time; }

    // --- Transform helpers ---
    Transform& transform() { return scene.get<Transform>(entity); }

    glm::vec3 position() { return transform().position; }
    void set_position(const glm::vec3& p) { transform().position = p; }

    glm::vec3 rotation() { return transform().rotation; }
    void set_rotation(const glm::vec3& r) { transform().rotation = r; }

    glm::vec3 get_scale() { return transform().scale; }
    void set_scale(const glm::vec3& s) { transform().scale = s; }

    glm::vec3 get_forward() {
        auto& r = transform().rotation;
        float yaw = glm::radians(r.y), pitch = glm::radians(r.x);
        return glm::normalize(glm::vec3(
            cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch)));
    }

    glm::vec3 get_right() {
        return glm::normalize(glm::cross(get_forward(), glm::vec3(0, 1, 0)));
    }

    glm::vec3 get_up() {
        return glm::normalize(glm::cross(get_right(), get_forward()));
    }

    void look_at(const glm::vec3& target) {
        glm::vec3 dir = glm::normalize(target - position());
        auto& r = transform().rotation;
        r.y = glm::degrees(atan2(dir.z, dir.x));
        r.x = glm::degrees(asin(glm::clamp(dir.y, -1.0f, 1.0f)));
    }

    void move(const glm::vec3& direction, float speed) {
        transform().position += direction * speed * delta_time;
    }

    // --- Entity management ---
    entt::entity create_entity(const std::string& name = "") {
        return scene.create_entity(name);
    }

    void destroy_entity(entt::entity e) { scene.destroy_entity(e); }

    entt::entity find_entity_by_name(const std::string& name) {
        auto view = scene.view<NameComponent>();
        for (auto e : view) {
            if (view.get<NameComponent>(e).name == name) return e;
        }
        return entt::null;
    }

    // --- Component access ---
    template<typename T>
    T& get_component(entt::entity e) { return scene.get<T>(e); }

    template<typename T>
    T& get_component() { return scene.get<T>(entity); }

    template<typename T>
    bool has_component(entt::entity e) { return scene.has<T>(e); }

    template<typename T>
    bool has_component() { return scene.has<T>(entity); }

    template<typename T, typename... Args>
    T& add_component(entt::entity e, Args&&... args) {
        return scene.add<T>(e, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& add_component(Args&&... args) {
        return scene.add<T>(entity, std::forward<Args>(args)...);
    }

    // --- Physics helpers ---
    void apply_force(const glm::vec3& force) {
        if (has_component<CharacterControllerComponent>()) {
            get_component<CharacterControllerComponent>().velocity += force * delta_time;
        }
    }

    void apply_impulse(const glm::vec3& impulse) {
        if (has_component<CharacterControllerComponent>()) {
            get_component<CharacterControllerComponent>().velocity += impulse;
        }
    }

    // --- Camera ---
    void set_active_camera(entt::entity cam_entity) {
        auto view = scene.view<CameraComponent>();
        for (auto e : view)
            view.get<CameraComponent>(e).primary = false;
        if (scene.has<CameraComponent>(cam_entity))
            scene.get<CameraComponent>(cam_entity).primary = true;
    }

    // --- Input ---
    bool key_down(int key) { return input && input->key_down(key); }
    bool key_pressed(int key) { return input && input->key_pressed(key); }
    bool key_released(int key) { return input && input->key_released(key); }
    bool mouse_down(int btn) { return input && input->mouse_down(btn); }
    bool mouse_pressed(int btn) { return input && input->mouse_pressed(btn); }
    double mouse_dx() { return input ? input->mouse_dx() : 0.0; }
    double mouse_dy() { return input ? input->mouse_dy() : 0.0; }
    double scroll_y() { return input ? input->scroll_y() : 0.0; }

    // --- Logging ---
    void log(const std::string& msg) { LOG_INFO("[Script] %s", msg.c_str()); }
    void log_warn(const std::string& msg) { LOG_WARN("[Script] %s", msg.c_str()); }
    void log_error(const std::string& msg) { LOG_ERROR("[Script] %s", msg.c_str()); }
};

// --- Base script class with full lifecycle callbacks ---

class LumiosScript {
public:
    virtual void on_create(ScriptContext& ctx) {}
    virtual void on_awake(ScriptContext& ctx) {}
    virtual void on_start(ScriptContext& ctx) {}
    virtual void on_update(ScriptContext& ctx) {}
    virtual void on_fixed_update(ScriptContext& ctx, float fixed_dt) {}
    virtual void on_late_update(ScriptContext& ctx) {}
    virtual void on_enable(ScriptContext& ctx) {}
    virtual void on_disable(ScriptContext& ctx) {}
    virtual void on_destroy(ScriptContext& ctx) {}
    virtual void on_collision_enter(ScriptContext& ctx, const CollisionInfo& info) {}
    virtual void on_collision_stay(ScriptContext& ctx, const CollisionInfo& info) {}
    virtual void on_collision_exit(ScriptContext& ctx, entt::entity other) {}
    virtual void on_trigger_enter(ScriptContext& ctx, entt::entity other) {}
    virtual void on_trigger_exit(ScriptContext& ctx, entt::entity other) {}
    virtual ~LumiosScript() = default;

    bool enabled = true;
};

} // namespace lumios

#ifdef _WIN32
#define LUMIOS_EXPORT extern "C" __declspec(dllexport)
#else
#define LUMIOS_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define LUMIOS_REGISTER_SCRIPT(ClassName) \
    LUMIOS_EXPORT lumios::LumiosScript* lumios_create_##ClassName() { return new ClassName(); } \
    LUMIOS_EXPORT void lumios_destroy_##ClassName(lumios::LumiosScript* s) { delete s; } \
    LUMIOS_EXPORT std::vector<lumios::PropertyInfo> lumios_properties_##ClassName() { \
        if constexpr (requires { ClassName::lumios_get_properties(); }) \
            return ClassName::lumios_get_properties(); \
        else return {}; \
    }
