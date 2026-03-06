#include "editor_panels.h"
#include "editor_renderer.h"
#include "scripting/script_manager.h"
#include "assets/loader.h"
#include "ImGuizmo.h"
#include <cstring>
#include <filesystem>
#include <fstream>

namespace lumios::editor {

// ─── Console log capture ────────────────────────────────────────────

struct LogEntry {
    LogLevel    level;
    std::string message;
};

static std::deque<LogEntry> s_log_entries;
static bool s_scroll_to_bottom = true;
static constexpr size_t MAX_LOG_ENTRIES = 2000;

static void log_capture(LogLevel level, const char* msg) {
    s_log_entries.push_back({level, msg});
    if (s_log_entries.size() > MAX_LOG_ENTRIES)
        s_log_entries.pop_front();
    s_scroll_to_bottom = true;
}

void init_console_log() {
    log::set_callback(log_capture);
}

// ─── Hierarchy panel ────────────────────────────────────────────────

void draw_hierarchy_panel(EditorState& state) {
    ImGui::Begin("Hierarchy");

    if (ImGui::Button("+ Entity")) {
        auto e = state.scene->create_entity("New Entity");
        state.selected = e;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Cube")) {
        auto e = state.scene->create_entity("Cube");
        state.scene->add<MeshComponent>(e, state.cube_mesh, state.default_mat);
        state.selected = e;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Sphere")) {
        auto e = state.scene->create_entity("Sphere");
        state.scene->add<MeshComponent>(e, state.sphere_mesh, state.default_mat);
        state.selected = e;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Light")) {
        auto e = state.scene->create_entity("Point Light");
        state.scene->get<Transform>(e).position = {0, 3, 0};
        state.scene->add<LightComponent>(e);
        state.selected = e;
    }

    ImGui::Separator();

    auto view = state.scene->view<Transform>();
    for (auto entity : view) {
        std::string label;
        if (state.scene->has<NameComponent>(entity))
            label = state.scene->get<NameComponent>(entity).name;
        else
            label = "Entity " + std::to_string(static_cast<u32>(entity));

        bool is_selected = (state.selected == entity);
        if (ImGui::Selectable(label.c_str(), is_selected))
            state.selected = entity;
    }

    // Delete with DEL key
    if (state.selected != entt::null && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        state.scene->destroy_entity(state.selected);
        state.selected = entt::null;
    }

    ImGui::End();
}

// ─── Inspector panel ────────────────────────────────────────────────

static bool draw_vec3(const char* label, glm::vec3& v, float reset = 0.0f) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
    ImGui::PushItemWidth(-1);
    changed = ImGui::DragFloat3("##v", &v.x, 0.1f);
    ImGui::PopItemWidth();
    ImGui::Columns(1);
    ImGui::PopID();
    return changed;
}

void draw_inspector_panel(EditorState& state) {
    ImGui::Begin("Inspector");

    if (state.selected == entt::null || !state.scene->registry().valid(state.selected)) {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    auto e = state.selected;

    // Name
    if (state.scene->has<NameComponent>(e)) {
        auto& name = state.scene->get<NameComponent>(e).name;
        char buf[256];
        strncpy(buf, name.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Name", buf, sizeof(buf)))
            name = buf;
    }

    ImGui::Separator();

    // Transform
    if (state.scene->has<Transform>(e)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& t = state.scene->get<Transform>(e);
            draw_vec3("Position", t.position);
            draw_vec3("Rotation", t.rotation);
            draw_vec3("Scale", t.scale, 1.0f);
        }
    }

    // Mesh component
    if (state.scene->has<MeshComponent>(e)) {
        if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& mc = state.scene->get<MeshComponent>(e);
            int mesh_idx = mc.mesh.valid() ? static_cast<int>(mc.mesh.index) : -1;
            const char* mesh_names[] = {"Cube", "Sphere", "Plane"};
            if (ImGui::Combo("Mesh", &mesh_idx, mesh_names, 3)) {
                MeshHandle handles[] = {state.cube_mesh, state.sphere_mesh, state.plane_mesh};
                if (mesh_idx >= 0 && mesh_idx < 3) mc.mesh = handles[mesh_idx];
            }
        }
        if (ImGui::SmallButton("Remove Mesh")) {
            state.scene->registry().remove<MeshComponent>(e);
        }
    } else {
        if (ImGui::SmallButton("+ Add Mesh"))
            state.scene->add<MeshComponent>(e, state.cube_mesh, state.default_mat);
    }

    // Camera component
    if (state.scene->has<CameraComponent>(e)) {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cam = state.scene->get<CameraComponent>(e);
            ImGui::DragFloat("FOV", &cam.fov, 1.0f, 5.0f, 179.0f);
            ImGui::DragFloat("Near", &cam.near_plane, 0.01f, 0.001f, 10.0f);
            ImGui::DragFloat("Far", &cam.far_plane, 10.0f, 10.0f, 100000.0f);
            ImGui::Checkbox("Primary", &cam.primary);
        }
        if (ImGui::SmallButton("Remove Camera"))
            state.scene->registry().remove<CameraComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add Camera"))
            state.scene->add<CameraComponent>(e);
    }

    // Script component
    if (state.scene->has<ScriptComponent>(e)) {
        if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sc = state.scene->get<ScriptComponent>(e);
            char buf[256];
            strncpy(buf, sc.script_class.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Class", buf, sizeof(buf)))
                sc.script_class = buf;

            // Render exposed properties from the script DLL
            if (state.script_manager && !sc.script_class.empty()) {
                auto& psets = state.script_manager->property_sets();
                auto it = psets.find(sc.script_class);
                if (it != psets.end()) {
                    LumiosScript* inst = state.script_manager->get_instance_for_entity(e);
                    if (inst) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Properties");
                        for (auto& prop : it->second.properties) {
                            char* base = reinterpret_cast<char*>(inst);
                            switch (prop.type) {
                                case PropertyType::Float:
                                    ImGui::DragFloat(prop.name, reinterpret_cast<float*>(base + prop.offset),
                                        0.1f, prop.min_val, prop.max_val);
                                    break;
                                case PropertyType::Int:
                                    ImGui::DragInt(prop.name, reinterpret_cast<int*>(base + prop.offset),
                                        1.0f, static_cast<int>(prop.min_val), static_cast<int>(prop.max_val));
                                    break;
                                case PropertyType::Bool:
                                    ImGui::Checkbox(prop.name, reinterpret_cast<bool*>(base + prop.offset));
                                    break;
                                case PropertyType::Vec3:
                                    ImGui::DragFloat3(prop.name, &reinterpret_cast<glm::vec3*>(base + prop.offset)->x, 0.1f);
                                    break;
                                default: break;
                            }
                        }
                    }
                }
            }
        }
        if (ImGui::SmallButton("Remove Script"))
            state.scene->registry().remove<ScriptComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add Script"))
            state.scene->add<ScriptComponent>(e);
    }

    // CharacterController component
    if (state.scene->has<CharacterControllerComponent>(e)) {
        if (ImGui::CollapsingHeader("Character Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cc = state.scene->get<CharacterControllerComponent>(e);
            ImGui::DragFloat("Move Speed", &cc.move_speed, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Sprint Multiplier", &cc.sprint_multiplier, 0.1f, 1.0f, 10.0f);
            ImGui::DragFloat("Jump Force", &cc.jump_force, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("Mouse Sensitivity", &cc.mouse_sensitivity, 0.01f, 0.01f, 2.0f);
            ImGui::DragFloat("Gravity Multiplier", &cc.gravity_multiplier, 0.1f, 0.0f, 10.0f);
            ImGui::Text("Grounded: %s", cc.is_grounded ? "Yes" : "No");
        }
        if (ImGui::SmallButton("Remove CharController"))
            state.scene->registry().remove<CharacterControllerComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add CharController"))
            state.scene->add<CharacterControllerComponent>(e);
    }

    // Rigidbody component
    if (state.scene->has<RigidbodyComponent>(e)) {
        if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& rb = state.scene->get<RigidbodyComponent>(e);
            int type = static_cast<int>(rb.type);
            const char* types[] = {"Static", "Dynamic", "Kinematic"};
            ImGui::Combo("Body Type", &type, types, 3);
            rb.type = static_cast<RigidbodyComponent::Type>(type);
            ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 10000.0f);
            ImGui::DragFloat("Linear Damping", &rb.linear_damping, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Angular Damping", &rb.angular_damping, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Use Gravity", &rb.use_gravity);
        }
        if (ImGui::SmallButton("Remove Rigidbody"))
            state.scene->registry().remove<RigidbodyComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add Rigidbody"))
            state.scene->add<RigidbodyComponent>(e);
    }

    // Collider component
    if (state.scene->has<ColliderComponent>(e)) {
        if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& col = state.scene->get<ColliderComponent>(e);
            int shape = static_cast<int>(col.shape);
            const char* shapes[] = {"Box", "Sphere", "Capsule", "Mesh", "Convex Hull"};
            ImGui::Combo("Shape", &shape, shapes, 5);
            col.shape = static_cast<ColliderComponent::Shape>(shape);

            if (col.shape == ColliderComponent::Shape::Box) {
                draw_vec3("Size", col.size, 1.0f);
            } else if (col.shape == ColliderComponent::Shape::Sphere) {
                ImGui::DragFloat("Radius", &col.radius, 0.01f, 0.01f, 100.0f);
            } else if (col.shape == ColliderComponent::Shape::Capsule) {
                ImGui::DragFloat("Radius", &col.radius, 0.01f, 0.01f, 100.0f);
                ImGui::DragFloat("Height", &col.height, 0.01f, 0.01f, 100.0f);
            } else if (col.shape == ColliderComponent::Shape::Mesh) {
                draw_vec3("Size", col.size, 1.0f);
                ImGui::TextDisabled("Uses entity mesh vertices");
            } else if (col.shape == ColliderComponent::Shape::ConvexHull) {
                draw_vec3("Size", col.size, 1.0f);
                ImGui::DragFloat("Hull Detail", &col.hull_detail, 0.01f, 0.05f, 1.0f, "%.2f");
                ImGui::TextDisabled("Vertices: %zu", col.hull_vertices.size());
            }

            draw_vec3("Offset", col.offset);
            ImGui::Checkbox("Is Trigger", &col.is_trigger);
            ImGui::DragFloat("Friction", &col.friction, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Restitution", &col.restitution, 0.01f, 0.0f, 2.0f);
        }
        if (ImGui::SmallButton("Remove Collider"))
            state.scene->registry().remove<ColliderComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add Collider"))
            state.scene->add<ColliderComponent>(e);
    }

    // Particle Emitter component
    if (state.scene->has<ParticleEmitterComponent>(e)) {
        if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& pe = state.scene->get<ParticleEmitterComponent>(e);
            int max_p = static_cast<int>(pe.max_particles);
            ImGui::DragInt("Max Particles", &max_p, 10, 1, 100000);
            pe.max_particles = static_cast<u32>(max_p);
            ImGui::DragFloat("Emit Rate", &pe.emit_rate, 1.0f, 0.0f, 10000.0f);
            ImGui::DragFloat("Lifetime", &pe.lifetime, 0.1f, 0.01f, 60.0f);
            draw_vec3("Velocity Min", pe.velocity_min);
            draw_vec3("Velocity Max", pe.velocity_max);
            ImGui::ColorEdit4("Color Start", &pe.color_start.x);
            ImGui::ColorEdit4("Color End", &pe.color_end.x);
            ImGui::DragFloat("Size Start", &pe.size_start, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Size End", &pe.size_end, 0.01f, 0.0f, 10.0f);
            draw_vec3("Gravity", pe.gravity);
        }
        if (ImGui::SmallButton("Remove Particles"))
            state.scene->registry().remove<ParticleEmitterComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add Particles"))
            state.scene->add<ParticleEmitterComponent>(e);
    }

    // Light component
    if (state.scene->has<LightComponent>(e)) {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& l = state.scene->get<LightComponent>(e);
            int type = static_cast<int>(l.type);
            const char* types[] = {"Directional", "Point", "Spot"};
            ImGui::Combo("Type", &type, types, 3);
            l.type = static_cast<LightType>(type);
            ImGui::ColorEdit3("Color", &l.color.x);
            ImGui::DragFloat("Intensity", &l.intensity, 0.05f, 0.0f, 100.0f);
            if (l.type != LightType::Directional) {
                ImGui::DragFloat("Range", &l.range, 0.5f, 0.1f, 500.0f);
                if (l.type == LightType::Spot)
                    ImGui::DragFloat("Spot Angle", &l.spot_angle, 1.0f, 1.0f, 90.0f);
            }
        }
        if (ImGui::SmallButton("Remove Light"))
            state.scene->registry().remove<LightComponent>(e);
    } else {
        if (ImGui::SmallButton("+ Add Light"))
            state.scene->add<LightComponent>(e);
    }

    ImGui::End();
}

// ─── Collider wireframe gizmos ──────────────────────────────────────

static ImVec2 world_to_screen(const glm::vec3& world, const glm::mat4& vp_mat,
                               ImVec2 rect_pos, ImVec2 rect_size) {
    glm::vec4 clip = vp_mat * glm::vec4(world, 1.0f);
    if (clip.w < 0.001f) return ImVec2(-1000, -1000);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return ImVec2(
        rect_pos.x + (ndc.x * 0.5f + 0.5f) * rect_size.x,
        rect_pos.y + (-ndc.y * 0.5f + 0.5f) * rect_size.y
    );
}

static void draw_wire_box(ImDrawList* dl, const glm::vec3& center, const glm::vec3& half,
                           const glm::mat4& vp, ImVec2 rp, ImVec2 rs, ImU32 col) {
    glm::vec3 corners[8];
    for (int i = 0; i < 8; i++) {
        corners[i] = center + glm::vec3(
            (i & 1) ? half.x : -half.x,
            (i & 2) ? half.y : -half.y,
            (i & 4) ? half.z : -half.z);
    }
    int edges[][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (auto& e : edges) {
        ImVec2 a = world_to_screen(corners[e[0]], vp, rp, rs);
        ImVec2 b = world_to_screen(corners[e[1]], vp, rp, rs);
        dl->AddLine(a, b, col, 1.5f);
    }
}

static void draw_wire_sphere(ImDrawList* dl, const glm::vec3& center, float radius,
                              const glm::mat4& vp, ImVec2 rp, ImVec2 rs, ImU32 col) {
    constexpr int SEGS = 32;
    auto ring = [&](int axis0, int axis1, int axis_up) {
        ImVec2 prev;
        for (int i = 0; i <= SEGS; i++) {
            float a = static_cast<float>(i) / SEGS * 2.0f * 3.14159265f;
            glm::vec3 pt = center;
            pt[axis0] += cos(a) * radius;
            pt[axis1] += sin(a) * radius;
            ImVec2 s = world_to_screen(pt, vp, rp, rs);
            if (i > 0) dl->AddLine(prev, s, col, 1.2f);
            prev = s;
        }
    };
    ring(0, 1, 2);
    ring(0, 2, 1);
    ring(1, 2, 0);
}

static void draw_wire_capsule(ImDrawList* dl, const glm::vec3& center, float radius, float height,
                               const glm::mat4& vp, ImVec2 rp, ImVec2 rs, ImU32 col) {
    float hh = height * 0.5f;
    glm::vec3 top = center + glm::vec3(0, hh, 0);
    glm::vec3 bot = center + glm::vec3(0, -hh, 0);
    constexpr int SEGS = 24;

    for (int ring = 0; ring < 2; ring++) {
        glm::vec3 c = ring == 0 ? top : bot;
        ImVec2 prev;
        for (int i = 0; i <= SEGS; i++) {
            float a = static_cast<float>(i) / SEGS * 2.0f * 3.14159265f;
            glm::vec3 pt = c + glm::vec3(cos(a) * radius, 0, sin(a) * radius);
            ImVec2 s = world_to_screen(pt, vp, rp, rs);
            if (i > 0) dl->AddLine(prev, s, col, 1.2f);
            prev = s;
        }
    }

    for (int i = 0; i < 4; i++) {
        float a = static_cast<float>(i) / 4.0f * 2.0f * 3.14159265f;
        glm::vec3 offset(cos(a) * radius, 0, sin(a) * radius);
        ImVec2 t = world_to_screen(top + offset, vp, rp, rs);
        ImVec2 b = world_to_screen(bot + offset, vp, rp, rs);
        dl->AddLine(t, b, col, 1.2f);
    }

    auto half_ring = [&](const glm::vec3& c, bool upper, int a0, int a1) {
        ImVec2 prev;
        for (int i = 0; i <= SEGS / 2; i++) {
            float a = static_cast<float>(i) / (SEGS / 2) * 3.14159265f;
            if (!upper) a = -a;
            glm::vec3 pt = c;
            pt[a0] += cos(a) * radius;
            pt.y += sin(a) * radius;
            ImVec2 s = world_to_screen(pt, vp, rp, rs);
            if (i > 0) dl->AddLine(prev, s, col, 1.2f);
            prev = s;
        }
    };
    half_ring(top, true, 0, 2);
    half_ring(top, true, 2, 0);
    half_ring(bot, false, 0, 2);
    half_ring(bot, false, 2, 0);
}

static void draw_collider_gizmos(EditorState& state, ImVec2 cursor_pos, ImVec2 avail) {
    if (!state.camera || !state.scene) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    glm::mat4 vp_mat = state.camera->projection() * state.camera->view();

    ImU32 col_box     = IM_COL32(50, 220, 80, 200);
    ImU32 col_sphere  = IM_COL32(50, 180, 220, 200);
    ImU32 col_capsule = IM_COL32(220, 180, 50, 200);
    ImU32 col_hull    = IM_COL32(200, 100, 220, 200);
    ImU32 col_mesh    = IM_COL32(220, 130, 50, 180);
    ImU32 col_trigger = IM_COL32(220, 220, 50, 150);

    auto view = state.scene->view<Transform, ColliderComponent>();
    for (auto entity : view) {
        bool is_selected = (entity == state.selected);
        auto& t = view.get<Transform>(entity);
        auto& col = view.get<ColliderComponent>(entity);
        glm::vec3 center = t.position + col.offset;

        ImU32 color;
        if (col.is_trigger) color = col_trigger;
        else {
            switch (col.shape) {
                case ColliderComponent::Shape::Sphere:     color = col_sphere; break;
                case ColliderComponent::Shape::Capsule:    color = col_capsule; break;
                case ColliderComponent::Shape::ConvexHull: color = col_hull; break;
                case ColliderComponent::Shape::Mesh:       color = col_mesh; break;
                default:                                   color = col_box; break;
            }
        }
        if (is_selected)
            color = (color & 0x00FFFFFF) | 0xFF000000;

        switch (col.shape) {
            case ColliderComponent::Shape::Box:
                draw_wire_box(dl, center, col.size * 0.5f, vp_mat, cursor_pos, avail, color);
                break;
            case ColliderComponent::Shape::Sphere:
                draw_wire_sphere(dl, center, col.radius, vp_mat, cursor_pos, avail, color);
                break;
            case ColliderComponent::Shape::Capsule:
                draw_wire_capsule(dl, center, col.radius, col.height, vp_mat, cursor_pos, avail, color);
                break;
            case ColliderComponent::Shape::ConvexHull:
                if (!col.hull_vertices.empty()) {
                    for (size_t i = 0; i < col.hull_vertices.size(); i++) {
                        for (size_t j = i + 1; j < col.hull_vertices.size(); j++) {
                            float d = glm::length(col.hull_vertices[i] - col.hull_vertices[j]);
                            if (d < col.size.x * 1.5f) {
                                ImVec2 a = world_to_screen(center + col.hull_vertices[i], vp_mat, cursor_pos, avail);
                                ImVec2 b = world_to_screen(center + col.hull_vertices[j], vp_mat, cursor_pos, avail);
                                dl->AddLine(a, b, color, 1.0f);
                            }
                        }
                    }
                } else {
                    draw_wire_box(dl, center, col.size * 0.5f, vp_mat, cursor_pos, avail, color);
                }
                break;
            case ColliderComponent::Shape::Mesh:
                draw_wire_box(dl, center, col.size * 0.5f, vp_mat, cursor_pos, avail, color);
                break;
        }
    }
}

// ─── Viewport panel ─────────────────────────────────────────────────

void draw_viewport_panel(EditorState& state, ImTextureID scene_texture, lumios::EditorRenderer* renderer) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    state.viewport_hovered = ImGui::IsWindowHovered();
    state.viewport_focused = ImGui::IsWindowFocused();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0 && avail.y > 0) {
        state.viewport_size = avail;
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        if (scene_texture)
            ImGui::Image(scene_texture, avail);

        draw_collider_gizmos(state, cursor_pos, avail);

        // Click-to-select: LMB click without Alt and without gizmo interaction
        if (renderer && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            state.viewport_hovered && !ImGui::GetIO().KeyAlt && !ImGuizmo::IsOver()) {
            ImVec2 mouse = ImGui::GetMousePos();
            u32 mx = static_cast<u32>(mouse.x - cursor_pos.x);
            u32 my = static_cast<u32>(mouse.y - cursor_pos.y);
            u32 pick_id = renderer->read_pick_pixel(mx, my);
            if (pick_id != UINT32_MAX) {
                entt::entity picked = static_cast<entt::entity>(pick_id);
                if (state.scene->registry().valid(picked))
                    state.selected = picked;
                else
                    state.selected = entt::null;
            } else {
                state.selected = entt::null;
            }
        }

        if (state.selected != entt::null && state.scene->registry().valid(state.selected)
            && state.scene->has<Transform>(state.selected) && state.camera) {

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(cursor_pos.x, cursor_pos.y, avail.x, avail.y);

            glm::mat4 view = state.camera->view();
            glm::mat4 proj = state.camera->projection();
            auto& t = state.scene->get<Transform>(state.selected);
            glm::mat4 model = t.matrix();

            float dist = glm::length(state.camera->position() - t.position);
            ImGuizmo::SetGizmoSizeClipSpace(glm::clamp(1.8f / glm::max(dist, 0.1f), 0.02f, 0.35f));

            ImGuizmo::OPERATION ops[] = {
                ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE
            };
            ImGuizmo::OPERATION op = ops[glm::clamp(state.gizmo_op, 0, 2)];

            if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0], op, ImGuizmo::WORLD,
                                     &model[0][0])) {
                glm::vec3 scl, pos;
                for (int i = 0; i < 3; i++)
                    scl[i] = glm::length(glm::vec3(model[i]));
                pos = glm::vec3(model[3]);

                glm::mat3 rot_mat;
                for (int i = 0; i < 3; i++)
                    rot_mat[i] = glm::vec3(model[i]) / scl[i];

                glm::vec3 rot;
                rot.x = glm::degrees(asin(glm::clamp(-rot_mat[1][2], -1.0f, 1.0f)));
                if (glm::abs(rot_mat[1][2]) < 0.999f) {
                    rot.y = glm::degrees(atan2(rot_mat[0][2], rot_mat[2][2]));
                    rot.z = glm::degrees(atan2(rot_mat[1][0], rot_mat[1][1]));
                } else {
                    rot.y = glm::degrees(atan2(-rot_mat[2][0], rot_mat[0][0]));
                    rot.z = 0.0f;
                }

                t.position = pos;
                t.rotation = rot;
                t.scale    = scl;
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─── Console panel ──────────────────────────────────────────────────

void draw_console_panel() {
    ImGui::Begin("Console");

    if (ImGui::SmallButton("Clear")) {
        s_log_entries.clear();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy All")) {
        std::string all;
        for (auto& entry : s_log_entries) {
            all += entry.message;
            all += '\n';
        }
        ImGui::SetClipboardText(all.c_str());
    }

    ImGui::Separator();
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(s_log_entries.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            auto& entry = s_log_entries[i];
            ImVec4 color;
            switch (entry.level) {
                case LogLevel::Trace: color = {0.5f, 0.5f, 0.5f, 1.0f}; break;
                case LogLevel::Debug: color = {0.4f, 0.8f, 0.9f, 1.0f}; break;
                case LogLevel::Info:  color = {0.5f, 0.9f, 0.5f, 1.0f}; break;
                case LogLevel::Warn:  color = {0.9f, 0.9f, 0.3f, 1.0f}; break;
                case LogLevel::Error: color = {0.9f, 0.3f, 0.3f, 1.0f}; break;
                case LogLevel::Fatal: color = {1.0f, 0.2f, 0.8f, 1.0f}; break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushID(i);
            if (ImGui::Selectable(entry.message.c_str())) {
                ImGui::SetClipboardText(entry.message.c_str());
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Click to copy");
            ImGui::PopID();
            ImGui::PopStyleColor();
        }
    }

    if (s_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        s_scroll_to_bottom = false;
    }

    ImGui::EndChild();
    ImGui::End();
}

// ─── Assets browser panel ──────────────────────────────────────────

static const char* get_file_icon(const std::string& ext) {
    if (ext == ".cpp" || ext == ".h" || ext == ".hpp") return "[C++]";
    if (ext == ".json") return "[JSON]";
    if (ext == ".png" || ext == ".jpg" || ext == ".bmp") return "[IMG]";
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf") return "[3D]";
    if (ext == ".dll" || ext == ".so") return "[DLL]";
    if (ext == ".vert" || ext == ".frag" || ext == ".comp") return "[GLSL]";
    return "[FILE]";
}

// Script template strings
static const std::unordered_map<std::string, std::string>& get_script_templates() {
    static std::unordered_map<std::string, std::string> templates = {
        {"Empty Script", R"(#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class %CLASS% : public lumios::LumiosScript {
public:
    void on_create(lumios::ScriptContext& ctx) override {
    }

    void on_update(lumios::ScriptContext& ctx) override {
        float dt = ctx.dt();
    }
};

LUMIOS_REGISTER_SCRIPT(%CLASS%)
)"},
        {"FPS Controller", R"(#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class %CLASS% : public lumios::LumiosScript {
    float move_speed_ = 5.0f;
    float sprint_mult_ = 2.5f;
    float mouse_sens_ = 0.15f;
    float jump_force_ = 5.0f;
    float yaw_ = 0.0f, pitch_ = 0.0f;
    glm::vec3 velocity_{0.0f};

    LUMIOS_PROPERTIES_BEGIN(%CLASS%)
        LUMIOS_PROP_FLOAT(move_speed_, 0.0f, 50.0f)
        LUMIOS_PROP_FLOAT(sprint_mult_, 1.0f, 10.0f)
        LUMIOS_PROP_FLOAT(mouse_sens_, 0.01f, 2.0f)
        LUMIOS_PROP_FLOAT(jump_force_, 0.0f, 30.0f)
    LUMIOS_PROPERTIES_END()

public:
    void on_start(lumios::ScriptContext& ctx) override {
        auto rot = ctx.rotation();
        yaw_ = rot.y; pitch_ = rot.x;
    }

    void on_update(lumios::ScriptContext& ctx) override {
        float dt = ctx.dt();
        float speed = move_speed_;
        if (ctx.key_down(GLFW_KEY_LEFT_SHIFT)) speed *= sprint_mult_;

        // Mouse look (while RMB held in game window)
        if (ctx.mouse_down(GLFW_MOUSE_BUTTON_RIGHT)) {
            yaw_ += static_cast<float>(ctx.mouse_dx()) * mouse_sens_;
            pitch_ -= static_cast<float>(ctx.mouse_dy()) * mouse_sens_;
            pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
        }

        glm::vec3 forward = ctx.get_forward();
        glm::vec3 right = ctx.get_right();

        glm::vec3 move{0.0f};
        if (ctx.key_down(GLFW_KEY_W)) move += forward;
        if (ctx.key_down(GLFW_KEY_S)) move -= forward;
        if (ctx.key_down(GLFW_KEY_A)) move -= right;
        if (ctx.key_down(GLFW_KEY_D)) move += right;
        if (glm::length(move) > 0.001f) move = glm::normalize(move);

        auto pos = ctx.position();
        pos += move * speed * dt;

        if (ctx.key_pressed(GLFW_KEY_SPACE))
            velocity_.y = jump_force_;

        velocity_.y -= 9.81f * dt;
        pos += velocity_ * dt;

        if (pos.y < 0.0f) { pos.y = 0.0f; velocity_.y = 0.0f; }

        ctx.set_position(pos);
        ctx.set_rotation(glm::vec3(pitch_, yaw_, 0.0f));
    }
};

LUMIOS_REGISTER_SCRIPT(%CLASS%)
)"},
        {"Rotator", R"(#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class %CLASS% : public lumios::LumiosScript {
    float speed_ = 90.0f;
    glm::vec3 axis_{0.0f, 1.0f, 0.0f};

    LUMIOS_PROPERTIES_BEGIN(%CLASS%)
        LUMIOS_PROP_FLOAT(speed_, -720.0f, 720.0f)
        LUMIOS_PROP_VEC3(axis_)
    LUMIOS_PROPERTIES_END()

public:
    void on_update(lumios::ScriptContext& ctx) override {
        auto rot = ctx.rotation();
        rot += axis_ * speed_ * ctx.dt();
        ctx.set_rotation(rot);
    }
};

LUMIOS_REGISTER_SCRIPT(%CLASS%)
)"},
        {"Follow Camera", R"(#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class %CLASS% : public lumios::LumiosScript {
    glm::vec3 offset_{0.0f, 3.0f, -5.0f};
    float smoothing_ = 5.0f;

    LUMIOS_PROPERTIES_BEGIN(%CLASS%)
        LUMIOS_PROP_VEC3(offset_)
        LUMIOS_PROP_FLOAT(smoothing_, 0.1f, 20.0f)
    LUMIOS_PROPERTIES_END()

public:
    void on_late_update(lumios::ScriptContext& ctx) override {
        auto target = ctx.find_entity_by_name("Player");
        if (target == entt::null) return;
        auto target_pos = ctx.get_component<lumios::Transform>(target).position;
        auto desired = target_pos + offset_;
        auto pos = ctx.position();
        pos = glm::mix(pos, desired, glm::clamp(smoothing_ * ctx.dt(), 0.0f, 1.0f));
        ctx.set_position(pos);
        ctx.look_at(target_pos);
    }
};

LUMIOS_REGISTER_SCRIPT(%CLASS%)
)"},
        {"Interactable", R"(#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class %CLASS% : public lumios::LumiosScript {
public:
    void on_collision_enter(lumios::ScriptContext& ctx, const lumios::CollisionInfo& info) override {
        ctx.log("Collision with entity!");
    }

    void on_collision_exit(lumios::ScriptContext& ctx, entt::entity other) override {
        ctx.log("Collision ended");
    }
};

LUMIOS_REGISTER_SCRIPT(%CLASS%)
)"},
        {"Trigger Zone", R"(#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class %CLASS% : public lumios::LumiosScript {
public:
    void on_trigger_enter(lumios::ScriptContext& ctx, entt::entity other) override {
        ctx.log("Entity entered trigger zone!");
    }

    void on_trigger_exit(lumios::ScriptContext& ctx, entt::entity other) override {
        ctx.log("Entity exited trigger zone");
    }
};

LUMIOS_REGISTER_SCRIPT(%CLASS%)
)"}
    };
    return templates;
}

static std::string filename_to_classname(const std::string& filename) {
    std::string name = filename;
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    std::string result;
    bool capitalize_next = true;
    for (char c : name) {
        if (c == '_' || c == '-' || c == ' ') { capitalize_next = true; continue; }
        if (capitalize_next) { result += static_cast<char>(toupper(c)); capitalize_next = false; }
        else result += c;
    }
    return result;
}

void draw_assets_panel(EditorState& state) {
    ImGui::Begin("Assets");

    namespace fs = std::filesystem;

    // Navigation breadcrumb
    {
        if (ImGui::SmallButton("assets")) state.current_assets_path = state.assets_root;
        std::string rel = state.current_assets_path.substr(state.assets_root.length());
        if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) rel = rel.substr(1);

        std::string building;
        for (auto& part : fs::path(rel)) {
            if (part.empty()) continue;
            building += "/" + part.string();
            ImGui::SameLine(); ImGui::Text(">");
            ImGui::SameLine();
            if (ImGui::SmallButton(part.string().c_str()))
                state.current_assets_path = state.assets_root + building;
        }
    }

    ImGui::Separator();

    // Right-click context menu on empty space
    if (ImGui::BeginPopupContextWindow("AssetsContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("New Script")) {
            for (auto& [name, templ] : get_script_templates()) {
                if (ImGui::MenuItem(name.c_str())) {
                    static char new_name[128] = "my_script";
                    std::string fname = std::string(new_name) + ".cpp";
                    std::string cname = filename_to_classname(new_name);
                    std::string content = templ;
                    size_t pos;
                    while ((pos = content.find("%CLASS%")) != std::string::npos)
                        content.replace(pos, 7, cname);
                    std::string path = state.current_assets_path + "/" + fname;
                    std::ofstream f(path);
                    if (f.is_open()) { f << content; f.close(); }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("New Folder")) {
            fs::create_directories(state.current_assets_path + "/New Folder");
        }
        if (ImGui::MenuItem("Open in Explorer")) {
#ifdef _WIN32
            std::string cmd = "explorer \"" + fs::absolute(state.current_assets_path).string() + "\"";
            std::system(cmd.c_str());
#endif
        }
        ImGui::EndPopup();
    }

    // File listing
    try {
        std::vector<fs::directory_entry> dirs, files;
        for (auto& entry : fs::directory_iterator(state.current_assets_path)) {
            if (entry.path().filename().string()[0] == '.') continue;
            if (entry.is_directory()) dirs.push_back(entry);
            else files.push_back(entry);
        }

        std::sort(dirs.begin(), dirs.end(), [](auto& a, auto& b) { return a.path().filename() < b.path().filename(); });
        std::sort(files.begin(), files.end(), [](auto& a, auto& b) { return a.path().filename() < b.path().filename(); });

        // Directories
        for (auto& dir : dirs) {
            std::string name = "[DIR] " + dir.path().filename().string();
            if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0))
                    state.current_assets_path = dir.path().string();
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rename")) {}
                if (ImGui::MenuItem("Delete")) {
                    try { fs::remove_all(dir.path()); } catch (...) {}
                }
                ImGui::EndPopup();
            }
        }

        // Files
        for (auto& file : files) {
            std::string ext = file.path().extension().string();
            std::string label = std::string(get_file_icon(ext)) + " " + file.path().filename().string();

            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
#ifdef _WIN32
                        std::string cmd = "start \"\" \"" + fs::absolute(file.path()).string() + "\"";
                        std::system(cmd.c_str());
#endif
                    } else if (ext == ".json" && file.path().string().find(".lumios.json") != std::string::npos) {
                        // Could load scene
                    }
                }
            }

            // Drag source for scripts -> attach to entities
            if (ext == ".cpp" && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string class_name = filename_to_classname(file.path().stem().string());
                ImGui::SetDragDropPayload("SCRIPT_CLASS", class_name.c_str(), class_name.size() + 1);
                ImGui::Text("Script: %s", class_name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    try { fs::remove(file.path()); } catch (...) {}
                }
                if (ImGui::MenuItem("Open in Explorer")) {
#ifdef _WIN32
                    std::string cmd = "explorer /select,\"" + fs::absolute(file.path()).string() + "\"";
                    std::system(cmd.c_str());
#endif
                }
                ImGui::EndPopup();
            }

            // Show file size
            ImGui::SameLine(ImGui::GetWindowWidth() - 80);
            auto size = file.file_size();
            if (size < 1024) ImGui::TextDisabled("%llu B", size);
            else ImGui::TextDisabled("%.1f KB", static_cast<float>(size) / 1024.0f);
        }
    } catch (...) {
        ImGui::TextDisabled("Cannot read directory");
    }

    // Accept drag-drop of scripts onto entities in hierarchy
    ImGui::End();
}

// ─── Script Reference panel ────────────────────────────────────────

void draw_script_reference_panel() {
    ImGui::Begin("Script Reference");

    if (ImGui::CollapsingHeader("Getting Started", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Scripts are C++ classes that inherit from LumiosScript. "
            "Place .cpp files in assets/scripts/. They are auto-compiled when saved.");
        ImGui::Spacing();
        ImGui::TextWrapped("Minimal script:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
        ImGui::TextUnformatted(
            "#include \"scripting/lumios_api.h\"\n"
            "\n"
            "class MyScript : public lumios::LumiosScript {\n"
            "    void on_update(lumios::ScriptContext& ctx) override {\n"
            "        float dt = ctx.dt();\n"
            "    }\n"
            "};\n"
            "LUMIOS_REGISTER_SCRIPT(MyScript)");
        ImGui::PopStyleColor();
    }

    if (ImGui::CollapsingHeader("Lifecycle Callbacks")) {
        ImGui::BulletText("on_awake(ctx)     - Called once when script is created");
        ImGui::BulletText("on_create(ctx)    - Called after awake");
        ImGui::BulletText("on_enable(ctx)    - Called when script is enabled");
        ImGui::BulletText("on_start(ctx)     - Called before first update");
        ImGui::BulletText("on_update(ctx)    - Called every frame");
        ImGui::BulletText("on_fixed_update(ctx, fixed_dt) - Fixed timestep");
        ImGui::BulletText("on_late_update(ctx) - After physics + update");
        ImGui::BulletText("on_disable(ctx)   - When script is disabled");
        ImGui::BulletText("on_destroy(ctx)   - When entity/script removed");
    }

    if (ImGui::CollapsingHeader("Collision Callbacks")) {
        ImGui::BulletText("on_collision_enter(ctx, info) - First frame of collision");
        ImGui::BulletText("on_collision_stay(ctx, info)  - Ongoing collision");
        ImGui::BulletText("on_collision_exit(ctx, other) - Collision ended");
        ImGui::BulletText("on_trigger_enter(ctx, other)  - Entered trigger");
        ImGui::BulletText("on_trigger_exit(ctx, other)   - Exited trigger");
        ImGui::Spacing();
        ImGui::TextWrapped("CollisionInfo has: other (entity), contact_point, normal, penetration");
    }

    if (ImGui::CollapsingHeader("ScriptContext API")) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Transform");
        ImGui::BulletText("position() / set_position(vec3)");
        ImGui::BulletText("rotation() / set_rotation(vec3)");
        ImGui::BulletText("get_scale() / set_scale(vec3)");
        ImGui::BulletText("get_forward() / get_right() / get_up()");
        ImGui::BulletText("look_at(vec3 target)");
        ImGui::BulletText("move(direction, speed) - delta-time aware");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Entity");
        ImGui::BulletText("create_entity(name) -> entity");
        ImGui::BulletText("destroy_entity(entity)");
        ImGui::BulletText("find_entity_by_name(name) -> entity");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Components");
        ImGui::BulletText("get_component<T>() / get_component<T>(entity)");
        ImGui::BulletText("has_component<T>() / has_component<T>(entity)");
        ImGui::BulletText("add_component<T>(args...) / add_component<T>(entity, args...)");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Physics");
        ImGui::BulletText("apply_force(vec3) - accelerate over time");
        ImGui::BulletText("apply_impulse(vec3) - instant velocity change");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Input");
        ImGui::BulletText("key_down(GLFW_KEY_*) / key_pressed / key_released");
        ImGui::BulletText("mouse_down(btn) / mouse_pressed(btn)");
        ImGui::BulletText("mouse_dx() / mouse_dy() / scroll_y()");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Camera");
        ImGui::BulletText("set_active_camera(entity) - switch game camera");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Utility");
        ImGui::BulletText("dt() - shorthand for delta_time");
        ImGui::BulletText("log(msg) / log_warn(msg) / log_error(msg)");
    }

    if (ImGui::CollapsingHeader("Exposing Properties")) {
        ImGui::TextWrapped(
            "Use macros to expose fields to the editor Inspector:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
        ImGui::TextUnformatted(
            "class MyScript : public lumios::LumiosScript {\n"
            "    float speed_ = 5.0f;\n"
            "    bool active_ = true;\n"
            "    glm::vec3 target_{0.0f};\n"
            "\n"
            "    LUMIOS_PROPERTIES_BEGIN(MyScript)\n"
            "        LUMIOS_PROP_FLOAT(speed_, 0.0f, 100.0f)\n"
            "        LUMIOS_PROP_BOOL(active_)\n"
            "        LUMIOS_PROP_VEC3(target_)\n"
            "    LUMIOS_PROPERTIES_END()\n"
            "    ...\n"
            "};");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextWrapped("Supported types: LUMIOS_PROP_FLOAT, LUMIOS_PROP_INT, LUMIOS_PROP_BOOL, LUMIOS_PROP_VEC3");
    }

    ImGui::End();
}

} // namespace lumios::editor
