#include "script_manager.h"
#include "../physics/physics_world.h"
#include "../core/log.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#endif

#include <filesystem>

namespace lumios {

void ScriptManager::init(Scene* scene, Input* input) {
    scene_ = scene;
    input_ = input;
}

void ScriptManager::shutdown() {
    destroy_all_instances();
    unload_dll();
}

uint64_t ScriptManager::get_file_time(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        ULARGE_INTEGER li;
        li.LowPart  = data.ftLastWriteTime.dwLowDateTime;
        li.HighPart = data.ftLastWriteTime.dwHighDateTime;
        return li.QuadPart;
    }
    return 0;
#else
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return static_cast<uint64_t>(st.st_mtime);
    return 0;
#endif
}

bool ScriptManager::load_dll(const std::string& path) {
    dll_path_ = path;

    std::string temp_path = path + ".loaded.dll";
    try {
        std::filesystem::copy_file(path, temp_path,
            std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        LOG_ERROR("ScriptManager: Failed to copy DLL %s", path.c_str());
        return false;
    }

#ifdef _WIN32
    dll_handle_ = LoadLibraryA(temp_path.c_str());
#else
    dll_handle_ = dlopen(temp_path.c_str(), RTLD_NOW);
#endif

    if (!dll_handle_) {
        LOG_ERROR("ScriptManager: Failed to load DLL %s", path.c_str());
        return false;
    }

    dll_last_write_ = get_file_time(path);
    resolve_symbols();
    LOG_INFO("ScriptManager: Loaded DLL %s (%zu script types)", path.c_str(), registered_scripts_.size());
    return true;
}

void ScriptManager::unload_dll() {
    if (!dll_handle_) return;
    destroy_all_instances();

#ifdef _WIN32
    FreeLibrary(dll_handle_);
#else
    dlclose(dll_handle_);
#endif
    dll_handle_ = nullptr;
    registered_scripts_.clear();
    property_sets_.clear();
    LOG_INFO("ScriptManager: DLL unloaded");
}

void ScriptManager::resolve_symbols() {
    registered_scripts_.clear();
    property_sets_.clear();
    if (!scene_) return;

    auto view = scene_->view<ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<ScriptComponent>(entity);
        if (sc.script_class.empty()) continue;
        if (registered_scripts_.count(sc.script_class)) continue;

        std::string create_name  = "lumios_create_"  + sc.script_class;
        std::string destroy_name = "lumios_destroy_" + sc.script_class;
        std::string props_name   = "lumios_properties_" + sc.script_class;

        ScriptInfo info;
        info.class_name = sc.script_class;

#ifdef _WIN32
        info.create    = reinterpret_cast<CreateFunc>(GetProcAddress(dll_handle_, create_name.c_str()));
        info.destroy   = reinterpret_cast<DestroyFunc>(GetProcAddress(dll_handle_, destroy_name.c_str()));
        info.get_props = reinterpret_cast<PropsFunc>(GetProcAddress(dll_handle_, props_name.c_str()));
#else
        info.create    = reinterpret_cast<CreateFunc>(dlsym(dll_handle_, create_name.c_str()));
        info.destroy   = reinterpret_cast<DestroyFunc>(dlsym(dll_handle_, destroy_name.c_str()));
        info.get_props = reinterpret_cast<PropsFunc>(dlsym(dll_handle_, props_name.c_str()));
#endif

        if (info.create && info.destroy) {
            registered_scripts_[sc.script_class] = info;
            LOG_INFO("ScriptManager: Registered script '%s'", sc.script_class.c_str());

            if (info.get_props) {
                ScriptPropertySet pset;
                pset.class_name = sc.script_class;
                pset.properties = info.get_props();
                if (!pset.properties.empty()) {
                    property_sets_[sc.script_class] = std::move(pset);
                    LOG_INFO("ScriptManager:   %zu properties exposed", property_sets_[sc.script_class].properties.size());
                }
            }
        } else {
            LOG_WARN("ScriptManager: Script '%s' not found in DLL", sc.script_class.c_str());
        }
    }
}

void ScriptManager::reload() {
    if (dll_path_.empty()) return;

    uint64_t current = get_file_time(dll_path_);
    if (current == dll_last_write_ || current == 0) return;

    LOG_INFO("ScriptManager: DLL changed, reloading...");
    unload_dll();
    load_dll(dll_path_);
    create_all_instances();
}

void ScriptManager::on_play() {
    create_all_instances();
}

void ScriptManager::on_stop() {
    destroy_all_instances();
}

void ScriptManager::create_all_instances() {
    destroy_all_instances();
    if (!dll_handle_ || !scene_) return;

    resolve_symbols();

    auto view = scene_->view<ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<ScriptComponent>(entity);
        auto it = registered_scripts_.find(sc.script_class);
        if (it == registered_scripts_.end()) continue;

        LumiosScript* instance = it->second.create();
        if (!instance) continue;

        ScriptContext ctx{*scene_, entity, 0.0f, input_};
        instance->on_awake(ctx);
        instance->on_create(ctx);

        live_instances_.push_back({entity, instance, it->second.destroy, false});
    }
}

void ScriptManager::destroy_all_instances() {
    if (!scene_) return;
    for (auto& li : live_instances_) {
        if (li.instance) {
            if (li.instance->enabled && scene_->registry().valid(li.entity)) {
                ScriptContext ctx{*scene_, li.entity, 0.0f, input_};
                li.instance->on_disable(ctx);
            }
            if (scene_->registry().valid(li.entity)) {
                ScriptContext ctx{*scene_, li.entity, 0.0f, input_};
                li.instance->on_destroy(ctx);
            }
            if (li.destroy) li.destroy(li.instance);
        }
    }
    live_instances_.clear();
}

void ScriptManager::update(float dt) {
    if (!scene_) return;
    for (auto& li : live_instances_) {
        if (!li.instance || !scene_->registry().valid(li.entity)) continue;
        if (!li.instance->enabled) continue;

        ScriptContext ctx{*scene_, li.entity, dt, input_};
        if (!li.started) {
            li.instance->on_enable(ctx);
            li.instance->on_start(ctx);
            li.started = true;
        }
        li.instance->on_update(ctx);
    }
}

void ScriptManager::fixed_update(float fixed_dt) {
    if (!scene_) return;
    for (auto& li : live_instances_) {
        if (!li.instance || !scene_->registry().valid(li.entity)) continue;
        if (!li.instance->enabled) continue;
        ScriptContext ctx{*scene_, li.entity, fixed_dt, input_};
        li.instance->on_fixed_update(ctx, fixed_dt);
    }
}

void ScriptManager::late_update(float dt) {
    if (!scene_) return;
    for (auto& li : live_instances_) {
        if (!li.instance || !scene_->registry().valid(li.entity)) continue;
        if (!li.instance->enabled) continue;
        ScriptContext ctx{*scene_, li.entity, dt, input_};
        li.instance->on_late_update(ctx);
    }
}

void ScriptManager::dispatch_collision_events(const PhysicsWorld& physics) {
    if (!scene_) return;

    for (auto& ci : physics.contact_infos()) {
        bool is_trigger = ci.event.is_trigger;

        for (auto& li : live_instances_) {
            if (!li.instance || !li.instance->enabled) continue;
            if (!scene_->registry().valid(li.entity)) continue;

            entt::entity other = entt::null;
            if (li.entity == ci.pair.a) other = ci.pair.b;
            else if (li.entity == ci.pair.b) other = ci.pair.a;
            else continue;

            ScriptContext ctx{*scene_, li.entity, 0.0f, input_};

            if (is_trigger) {
                if (ci.state == PhysicsWorld::ContactState::Enter)
                    li.instance->on_trigger_enter(ctx, other);
                else if (ci.state == PhysicsWorld::ContactState::Exit)
                    li.instance->on_trigger_exit(ctx, other);
            } else {
                CollisionInfo info;
                info.other = other;
                info.contact_point = ci.event.contact_point;
                info.normal = ci.event.normal;
                info.penetration = ci.event.penetration;

                if (ci.state == PhysicsWorld::ContactState::Enter)
                    li.instance->on_collision_enter(ctx, info);
                else if (ci.state == PhysicsWorld::ContactState::Stay)
                    li.instance->on_collision_stay(ctx, info);
                else if (ci.state == PhysicsWorld::ContactState::Exit)
                    li.instance->on_collision_exit(ctx, other);
            }
        }
    }
}

LumiosScript* ScriptManager::get_instance_for_entity(entt::entity e) {
    for (auto& li : live_instances_) {
        if (li.entity == e && li.instance) return li.instance;
    }
    return nullptr;
}

} // namespace lumios
