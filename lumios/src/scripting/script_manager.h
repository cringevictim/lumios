#pragma once

#include "lumios_api.h"
#include "../scene/scene.h"
#include "../scene/components.h"
#include "../physics/physics_components.h"
#include "../core/input.h"
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lumios {

class PhysicsWorld;

class ScriptManager {
public:
    void init(Scene* scene, Input* input);
    void shutdown();

    bool load_dll(const std::string& path);
    void unload_dll();
    void reload();

    void on_play();
    void on_stop();
    void update(float dt);
    void fixed_update(float fixed_dt);
    void late_update(float dt);
    void dispatch_collision_events(const PhysicsWorld& physics);

    bool is_loaded() const { return dll_handle_ != nullptr; }
    const std::string& dll_path() const { return dll_path_; }

    // Property access for the editor inspector
    struct ScriptPropertySet {
        std::string class_name;
        std::vector<PropertyInfo> properties;
    };
    const std::unordered_map<std::string, ScriptPropertySet>& property_sets() const { return property_sets_; }
    LumiosScript* get_instance_for_entity(entt::entity e);

private:
    Scene* scene_  = nullptr;
    Input* input_  = nullptr;

    std::string dll_path_;
    uint64_t    dll_last_write_ = 0;

#ifdef _WIN32
    HMODULE dll_handle_ = nullptr;
#else
    void* dll_handle_ = nullptr;
#endif

    using CreateFunc  = LumiosScript*(*)();
    using DestroyFunc = void(*)(LumiosScript*);
    using PropsFunc   = std::vector<PropertyInfo>(*)();

    struct ScriptInfo {
        std::string class_name;
        CreateFunc  create   = nullptr;
        DestroyFunc destroy  = nullptr;
        PropsFunc   get_props = nullptr;
    };

    std::unordered_map<std::string, ScriptInfo> registered_scripts_;
    std::unordered_map<std::string, ScriptPropertySet> property_sets_;

    struct LiveInstance {
        entt::entity entity;
        LumiosScript* instance = nullptr;
        DestroyFunc   destroy  = nullptr;
        bool started = false;
    };
    std::vector<LiveInstance> live_instances_;

    void destroy_all_instances();
    void create_all_instances();
    uint64_t get_file_time(const std::string& path);
    void resolve_symbols();
};

} // namespace lumios
