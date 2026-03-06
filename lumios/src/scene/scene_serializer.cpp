#include "scene_serializer.h"
#include "components.h"
#include "../core/log.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace lumios {

static json vec3_to_json(const glm::vec3& v) {
    return {v.x, v.y, v.z};
}

static glm::vec3 json_to_vec3(const json& j, glm::vec3 def = glm::vec3(0.0f)) {
    if (!j.is_array() || j.size() < 3) return def;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

static json vec4_to_json(const glm::vec4& v) {
    return {v.x, v.y, v.z, v.w};
}

static glm::vec4 json_to_vec4(const json& j, glm::vec4 def = glm::vec4(1.0f)) {
    if (!j.is_array() || j.size() < 4) return def;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

std::string SceneSerializer::serialize(const Scene& scene) {
    json root;
    json entities = json::array();

    auto view = scene.view<Transform>();
    for (auto entity : view) {
        json e_json;
        e_json["id"] = static_cast<u32>(entity);

        if (scene.has<NameComponent>(entity))
            e_json["name"] = scene.get<NameComponent>(entity).name;

        json components;

        // Transform
        {
            auto& t = scene.get<Transform>(entity);
            components["Transform"] = {
                {"position", vec3_to_json(t.position)},
                {"rotation", vec3_to_json(t.rotation)},
                {"scale",    vec3_to_json(t.scale)}
            };
        }

        // MeshComponent
        if (scene.has<MeshComponent>(entity)) {
            auto& mc = scene.get<MeshComponent>(entity);
            components["MeshComponent"] = {
                {"mesh_index",     mc.mesh.valid() ? static_cast<int>(mc.mesh.index) : -1},
                {"material_index", mc.material.valid() ? static_cast<int>(mc.material.index) : -1}
            };
        }

        // LightComponent
        if (scene.has<LightComponent>(entity)) {
            auto& l = scene.get<LightComponent>(entity);
            components["LightComponent"] = {
                {"type",       static_cast<int>(l.type)},
                {"color",      vec3_to_json(l.color)},
                {"intensity",  l.intensity},
                {"range",      l.range},
                {"spot_angle", l.spot_angle}
            };
        }

        // CameraComponent
        if (scene.has<CameraComponent>(entity)) {
            auto& cam = scene.get<CameraComponent>(entity);
            components["CameraComponent"] = {
                {"fov",        cam.fov},
                {"near_plane", cam.near_plane},
                {"far_plane",  cam.far_plane},
                {"primary",    cam.primary}
            };
        }

        // ScriptComponent
        if (scene.has<ScriptComponent>(entity)) {
            auto& sc = scene.get<ScriptComponent>(entity);
            components["ScriptComponent"] = {
                {"script_class", sc.script_class}
            };
        }

        // RigidbodyComponent
        if (scene.has<RigidbodyComponent>(entity)) {
            auto& rb = scene.get<RigidbodyComponent>(entity);
            components["RigidbodyComponent"] = {
                {"type",            static_cast<int>(rb.type)},
                {"mass",            rb.mass},
                {"linear_damping",  rb.linear_damping},
                {"angular_damping", rb.angular_damping},
                {"use_gravity",     rb.use_gravity}
            };
        }

        // ColliderComponent
        if (scene.has<ColliderComponent>(entity)) {
            auto& col = scene.get<ColliderComponent>(entity);
            components["ColliderComponent"] = {
                {"shape",       static_cast<int>(col.shape)},
                {"size",        vec3_to_json(col.size)},
                {"offset",      vec3_to_json(col.offset)},
                {"radius",      col.radius},
                {"height",      col.height},
                {"friction",    col.friction},
                {"restitution", col.restitution},
                {"is_trigger",  col.is_trigger},
                {"hull_detail", col.hull_detail}
            };
        }

        // CharacterControllerComponent
        if (scene.has<CharacterControllerComponent>(entity)) {
            auto& cc = scene.get<CharacterControllerComponent>(entity);
            components["CharacterControllerComponent"] = {
                {"move_speed",         cc.move_speed},
                {"sprint_multiplier",  cc.sprint_multiplier},
                {"jump_force",         cc.jump_force},
                {"mouse_sensitivity",  cc.mouse_sensitivity},
                {"gravity_multiplier", cc.gravity_multiplier}
            };
        }

        // ParticleEmitterComponent
        if (scene.has<ParticleEmitterComponent>(entity)) {
            auto& pe = scene.get<ParticleEmitterComponent>(entity);
            components["ParticleEmitterComponent"] = {
                {"max_particles", pe.max_particles},
                {"emit_rate",     pe.emit_rate},
                {"lifetime",      pe.lifetime},
                {"velocity_min",  vec3_to_json(pe.velocity_min)},
                {"velocity_max",  vec3_to_json(pe.velocity_max)},
                {"color_start",   vec4_to_json(pe.color_start)},
                {"color_end",     vec4_to_json(pe.color_end)},
                {"size_start",    pe.size_start},
                {"size_end",      pe.size_end},
                {"gravity",       vec3_to_json(pe.gravity)}
            };
        }

        e_json["components"] = components;
        entities.push_back(e_json);
    }

    root["entities"] = entities;
    root["version"]  = 1;
    return root.dump(2);
}

bool SceneSerializer::deserialize(Scene& scene, const std::string& json_str) {
    try {
        json root = json::parse(json_str);
        scene.clear();

        for (auto& e_json : root["entities"]) {
            std::string name = e_json.value("name", "Entity");
            auto entity = scene.create_entity(name);
            auto& comps = e_json["components"];

            if (comps.contains("Transform")) {
                auto& t = scene.get<Transform>(entity);
                auto& tj = comps["Transform"];
                t.position = json_to_vec3(tj["position"]);
                t.rotation = json_to_vec3(tj["rotation"]);
                t.scale    = json_to_vec3(tj["scale"], glm::vec3(1.0f));
            }

            if (comps.contains("MeshComponent")) {
                auto& mj = comps["MeshComponent"];
                MeshHandle mh{};
                MaterialHandle mat{};
                int mi = mj.value("mesh_index", -1);
                int mati = mj.value("material_index", -1);
                if (mi >= 0)   mh.index  = static_cast<u32>(mi);
                if (mati >= 0) mat.index = static_cast<u32>(mati);
                scene.add<MeshComponent>(entity, mh, mat);
            }

            if (comps.contains("LightComponent")) {
                auto& lj = comps["LightComponent"];
                LightComponent l;
                l.type       = static_cast<LightType>(lj.value("type", 1));
                l.color      = json_to_vec3(lj["color"], glm::vec3(1.0f));
                l.intensity  = lj.value("intensity", 1.0f);
                l.range      = lj.value("range", 20.0f);
                l.spot_angle = lj.value("spot_angle", 45.0f);
                scene.add<LightComponent>(entity) = l;
            }

            if (comps.contains("CameraComponent")) {
                auto& cj = comps["CameraComponent"];
                CameraComponent cam;
                cam.fov        = cj.value("fov", 60.0f);
                cam.near_plane = cj.value("near_plane", 0.1f);
                cam.far_plane  = cj.value("far_plane", 1000.0f);
                cam.primary    = cj.value("primary", false);
                scene.add<CameraComponent>(entity) = cam;
            }

            if (comps.contains("ScriptComponent")) {
                auto& sj = comps["ScriptComponent"];
                ScriptComponent sc;
                sc.script_class = sj.value("script_class", "");
                scene.add<ScriptComponent>(entity) = sc;
            }

            if (comps.contains("RigidbodyComponent")) {
                auto& rj = comps["RigidbodyComponent"];
                RigidbodyComponent rb;
                rb.type            = static_cast<RigidbodyComponent::Type>(rj.value("type", 1));
                rb.mass            = rj.value("mass", 1.0f);
                rb.linear_damping  = rj.value("linear_damping", 0.05f);
                rb.angular_damping = rj.value("angular_damping", 0.05f);
                rb.use_gravity     = rj.value("use_gravity", true);
                scene.add<RigidbodyComponent>(entity) = rb;
            }

            if (comps.contains("ColliderComponent")) {
                auto& cj = comps["ColliderComponent"];
                ColliderComponent col;
                col.shape       = static_cast<ColliderComponent::Shape>(cj.value("shape", 0));
                col.size        = json_to_vec3(cj.contains("size") ? cj["size"] : json(), glm::vec3(1.0f));
                col.offset      = json_to_vec3(cj.contains("offset") ? cj["offset"] : json(), glm::vec3(0.0f));
                col.radius      = cj.value("radius", 0.5f);
                col.height      = cj.value("height", 1.0f);
                col.friction    = cj.value("friction", 0.5f);
                col.restitution = cj.value("restitution", 0.3f);
                col.is_trigger  = cj.value("is_trigger", false);
                col.hull_detail = cj.value("hull_detail", 0.5f);
                scene.add<ColliderComponent>(entity) = col;
            }

            if (comps.contains("CharacterControllerComponent")) {
                auto& cj = comps["CharacterControllerComponent"];
                CharacterControllerComponent cc;
                cc.move_speed         = cj.value("move_speed", 5.0f);
                cc.sprint_multiplier  = cj.value("sprint_multiplier", 2.0f);
                cc.jump_force         = cj.value("jump_force", 5.0f);
                cc.mouse_sensitivity  = cj.value("mouse_sensitivity", 0.15f);
                cc.gravity_multiplier = cj.value("gravity_multiplier", 1.0f);
                scene.add<CharacterControllerComponent>(entity) = cc;
            }

            if (comps.contains("ParticleEmitterComponent")) {
                auto& pj = comps["ParticleEmitterComponent"];
                ParticleEmitterComponent pe;
                pe.max_particles = pj.value("max_particles", 1000u);
                pe.emit_rate     = pj.value("emit_rate", 100.0f);
                pe.lifetime      = pj.value("lifetime", 2.0f);
                pe.velocity_min  = json_to_vec3(pj["velocity_min"], glm::vec3(-1.0f));
                pe.velocity_max  = json_to_vec3(pj["velocity_max"], glm::vec3(1.0f));
                pe.color_start   = json_to_vec4(pj["color_start"]);
                pe.color_end     = json_to_vec4(pj["color_end"]);
                pe.size_start    = pj.value("size_start", 0.1f);
                pe.size_end      = pj.value("size_end", 0.0f);
                pe.gravity       = json_to_vec3(pj["gravity"], glm::vec3(0, -9.8f, 0));
                scene.add<ParticleEmitterComponent>(entity) = pe;
            }
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to deserialize scene: %s", e.what());
        return false;
    }
}

bool SceneSerializer::save(const Scene& scene, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: %s", path.c_str());
        return false;
    }
    file << serialize(scene);
    file.close();
    LOG_INFO("Scene saved to %s", path.c_str());
    return true;
}

bool SceneSerializer::load(Scene& scene, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open scene file: %s", path.c_str());
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    bool result = deserialize(scene, content);
    if (result) LOG_INFO("Scene loaded from %s", path.c_str());
    return result;
}

} // namespace lumios
