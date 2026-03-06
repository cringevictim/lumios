#include "editor_app.h"
#include "assets/loader.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "imgui_internal.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace lumios::editor {

bool EditorApp::init() {
    log::init();
    init_console_log();
    LOG_INFO("Lumios Editor starting...");

    WindowConfig wc;
    wc.title  = "Lumios Editor";
    wc.width  = 1920;
    wc.height = 1080;

    if (!window_.init(wc, events_)) return false;
    input_.init(window_.handle());
    timer_.reset();

    if (!renderer_.init(window_, LUMIOS_SHADER_DIR)) return false;

    state_.scene  = &scene_;
    state_.camera = &editor_camera_;
    state_.cube_mesh   = renderer_.upload_mesh(assets::create_cube());
    state_.sphere_mesh = renderer_.upload_mesh(assets::create_sphere(32, 16, 0.5f));
    state_.plane_mesh  = renderer_.upload_mesh(assets::create_plane(30.0f, 4));
    MaterialData mat_data{};
    mat_data.base_color = {0.8f, 0.8f, 0.8f, 1.0f};
    mat_data.roughness  = 0.6f;
    state_.default_mat = renderer_.create_material(mat_data);

    script_manager_.init(&scene_, &input_);
    physics_world_.init();
    state_.script_manager = &script_manager_;

    load_recent_projects();
    setup_default_scene();
    update_orbit_camera();

    ImGuiIO& io = ImGui::GetIO();
    layout_initialized_ = io.IniFilename && std::ifstream(io.IniFilename).good();

    running_ = true;
    LOG_INFO("Editor initialized");
    return true;
}

void EditorApp::setup_default_scene() {
    MaterialData red_d{}; red_d.base_color = {0.9f, 0.15f, 0.1f, 1.0f}; red_d.metallic = 0.3f; red_d.roughness = 0.4f;
    MaterialData blue_d{}; blue_d.base_color = {0.1f, 0.3f, 0.9f, 1.0f}; blue_d.metallic = 0.7f; blue_d.roughness = 0.2f;
    auto red  = renderer_.create_material(red_d);
    auto blue = renderer_.create_material(blue_d);

    // Ground
    auto ground = scene_.create_entity("Ground");
    scene_.get<Transform>(ground).position = {0, -0.5f, 0};
    scene_.get<Transform>(ground).scale = {20, 0.1f, 20};
    scene_.add<MeshComponent>(ground, state_.cube_mesh, state_.default_mat);

    // Center sphere
    auto sphere = scene_.create_entity("Sphere");
    scene_.get<Transform>(sphere).position = {0, 0.5f, 0};
    scene_.add<MeshComponent>(sphere, state_.sphere_mesh, blue);

    // Cubes
    for (int i = 0; i < 4; i++) {
        float angle = static_cast<float>(i) / 4.0f * 2.0f * PI;
        auto cube = scene_.create_entity("Cube " + std::to_string(i));
        scene_.get<Transform>(cube).position = {cos(angle) * 3.0f, 0.5f, sin(angle) * 3.0f};
        scene_.add<MeshComponent>(cube, state_.cube_mesh, (i % 2 == 0) ? red : blue);
    }

    // Sun
    auto sun = scene_.create_entity("Directional Light");
    scene_.get<Transform>(sun).rotation = {-45.0f, 30.0f, 0};
    scene_.add<LightComponent>(sun, LightType::Directional, glm::vec3(1, 0.95f, 0.85f), 1.2f);

    // Point light
    auto pl = scene_.create_entity("Point Light");
    scene_.get<Transform>(pl).position = {2, 3, 2};
    scene_.add<LightComponent>(pl, LightType::Point, glm::vec3(1, 0.6f, 0.3f), 2.0f, 15.0f);
}

void EditorApp::update_orbit_camera() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(orbit_yaw_)) * cos(glm::radians(orbit_pitch_));
    dir.y = sin(glm::radians(orbit_pitch_));
    dir.z = sin(glm::radians(orbit_yaw_)) * cos(glm::radians(orbit_pitch_));
    editor_camera_.set_position(focus_point_ - glm::normalize(dir) * orbit_distance_);
    editor_camera_.look_at(focus_point_);
    editor_camera_.set_aspect(state_.viewport_size.x / std::max(state_.viewport_size.y, 1.0f));
}

void EditorApp::update_editor_camera(float dt) {
    bool rmb  = input_.mouse_down(GLFW_MOUSE_BUTTON_RIGHT);
    bool mmb  = input_.mouse_down(GLFW_MOUSE_BUTTON_MIDDLE);

    // Start capture when a mouse button is pressed while viewport is hovered
    if (state_.viewport_hovered && (input_.mouse_pressed(GLFW_MOUSE_BUTTON_RIGHT) ||
                                     input_.mouse_pressed(GLFW_MOUSE_BUTTON_MIDDLE)))
        viewport_captured_ = true;

    // Release capture when all buttons are up
    if (!rmb && !mmb)
        viewport_captured_ = false;

    bool active = state_.viewport_hovered || viewport_captured_;
    if (!active) return;

    float dx  = static_cast<float>(input_.mouse_dx());
    float dy  = static_cast<float>(input_.mouse_dy());

    // RMB hold: FPS-style camera look
    if (rmb) {
        orbit_yaw_   += dx * 0.15f;
        orbit_pitch_ += dy * 0.15f;
        orbit_pitch_  = glm::clamp(orbit_pitch_, -89.0f, 89.0f);
    }

    // MMB: Pan
    if (mmb) {
        float pan_speed = orbit_distance_ * 0.002f;
        glm::vec3 right_v = glm::normalize(glm::cross(editor_camera_.front(), glm::vec3(0, 1, 0)));
        glm::vec3 up_v    = glm::normalize(glm::cross(right_v, editor_camera_.front()));
        focus_point_ -= right_v * dx * pan_speed;
        focus_point_ += up_v    * dy * pan_speed;
    }

    // Scroll wheel: Zoom
    if (state_.viewport_hovered) {
        float scroll = static_cast<float>(input_.scroll_y());
        if (scroll != 0.0f) {
            orbit_distance_ *= (1.0f - scroll * 0.08f);
            orbit_distance_  = glm::clamp(orbit_distance_, 0.5f, 500.0f);
        }
    }

    // WASD movement (always active when viewport is hovered or captured)
    float speed = 6.0f * dt;
    if (input_.key_down(GLFW_KEY_LEFT_CONTROL)) speed *= 3.0f;
    glm::vec3 fwd     = editor_camera_.front();
    glm::vec3 right_v = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
    if (input_.key_down(GLFW_KEY_W)) focus_point_ += fwd     * speed;
    if (input_.key_down(GLFW_KEY_S)) focus_point_ -= fwd     * speed;
    if (input_.key_down(GLFW_KEY_A)) focus_point_ -= right_v * speed;
    if (input_.key_down(GLFW_KEY_D)) focus_point_ += right_v * speed;
    if (input_.key_down(GLFW_KEY_SPACE))      focus_point_.y += speed;
    if (input_.key_down(GLFW_KEY_LEFT_SHIFT)) focus_point_.y -= speed;

    // F: Focus selected entity
    if (input_.key_pressed(GLFW_KEY_F) && state_.selected != entt::null &&
        scene_.registry().valid(state_.selected)) {
        focus_point_ = scene_.get<Transform>(state_.selected).position;
        orbit_distance_ = glm::max(2.0f, glm::length(scene_.get<Transform>(state_.selected).scale) * 3.0f);
    }

    // Numpad views
    if (input_.key_pressed(GLFW_KEY_KP_1)) { orbit_yaw_ = -90.0f; orbit_pitch_ = 0.0f; }
    if (input_.key_pressed(GLFW_KEY_KP_3)) { orbit_yaw_ = 0.0f;   orbit_pitch_ = 0.0f; }
    if (input_.key_pressed(GLFW_KEY_KP_7)) { orbit_yaw_ = -90.0f; orbit_pitch_ = -89.0f; }

    // Gizmo hotkeys (1/2/3)
    if (input_.key_pressed(GLFW_KEY_1)) gizmo_op_ = 0;
    if (input_.key_pressed(GLFW_KEY_2)) gizmo_op_ = 1;
    if (input_.key_pressed(GLFW_KEY_3)) gizmo_op_ = 2;

    update_orbit_camera();
}

void EditorApp::save_scene(const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    SceneSerializer::save(scene_, path);
    current_scene_path_ = path;
}

void EditorApp::load_scene(const std::string& path) {
    SceneSerializer::load(scene_, path);
    current_scene_path_ = path;
    state_.selected = entt::null;
}

void EditorApp::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene"))    { scene_.clear(); state_.selected = entt::null; current_scene_path_.clear(); setup_default_scene(); }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (current_scene_path_.empty()) current_scene_path_ = "assets/scenes/scene.lumios.json";
                save_scene(current_scene_path_);
            }
            if (ImGui::MenuItem("Save As...")) {
                save_scene("assets/scenes/scene.lumios.json");
            }
            if (ImGui::MenuItem("Load Scene...")) {
                if (std::filesystem::exists("assets/scenes/scene.lumios.json"))
                    load_scene("assets/scenes/scene.lumios.json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))         running_ = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Entity")) {
            if (ImGui::MenuItem("Empty"))        { state_.selected = scene_.create_entity("Empty"); }
            if (ImGui::MenuItem("Cube"))         { auto e = scene_.create_entity("Cube");   scene_.add<MeshComponent>(e, state_.cube_mesh,   state_.default_mat); state_.selected = e; }
            if (ImGui::MenuItem("Sphere"))       { auto e = scene_.create_entity("Sphere"); scene_.add<MeshComponent>(e, state_.sphere_mesh, state_.default_mat); state_.selected = e; }
            if (ImGui::MenuItem("Plane"))        { auto e = scene_.create_entity("Plane");  scene_.add<MeshComponent>(e, state_.plane_mesh,  state_.default_mat); state_.selected = e; }
            ImGui::Separator();
            if (ImGui::MenuItem("Camera"))       { auto e = scene_.create_entity("Camera");  scene_.add<CameraComponent>(e); state_.selected = e; }
            ImGui::Separator();
            if (ImGui::MenuItem("FPS Player")) {
                auto e = scene_.create_entity("Player");
                scene_.get<Transform>(e).position = {0, 1, 0};
                scene_.add<MeshComponent>(e, state_.sphere_mesh, state_.default_mat);
                CameraComponent cam; cam.primary = true;
                scene_.add<CameraComponent>(e) = cam;
                CharacterControllerComponent cc;
                scene_.add<CharacterControllerComponent>(e) = cc;
                RigidbodyComponent rb; rb.type = RigidbodyComponent::Type::Dynamic;
                scene_.add<RigidbodyComponent>(e) = rb;
                ColliderComponent col; col.shape = ColliderComponent::Shape::Capsule;
                col.radius = 0.4f; col.height = 1.8f;
                scene_.add<ColliderComponent>(e) = col;
                ScriptComponent sc; sc.script_class = "FpsController";
                scene_.add<ScriptComponent>(e) = sc;
                state_.selected = e;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Dir Light"))    { auto e = scene_.create_entity("Dir Light"); scene_.add<LightComponent>(e, LightType::Directional); state_.selected = e; }
            if (ImGui::MenuItem("Point Light"))  { auto e = scene_.create_entity("Point Light"); scene_.get<Transform>(e).position = {0,3,0}; scene_.add<LightComponent>(e); state_.selected = e; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Project")) {
            if (ImGui::MenuItem("New Project...")) {
                std::string folder = (std::filesystem::current_path() / "projects" / "new_project").string();
                create_new_project(folder);
            }
            if (ImGui::MenuItem("Open Project...")) {
                std::string proj_path = "project.lumios";
                if (std::filesystem::exists(proj_path))
                    load_project(proj_path);
            }
            if (!recent_projects_.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Recent Projects");
                for (auto& rp : recent_projects_) {
                    if (ImGui::MenuItem(rp.c_str()))
                        load_project(rp);
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Project Settings")) {
                ImGui::DragFloat3("Gravity", &project_.gravity.x, 0.1f);
                ImGui::DragFloat("Fixed Timestep", &project_.fixed_timestep, 0.001f, 0.001f, 0.1f, "%.4f");
                ImGui::Checkbox("Bloom", &project_.enable_bloom);
                ImGui::Checkbox("SSAO", &project_.enable_ssao);
                ImGui::Checkbox("Shadows", &project_.enable_shadows);
                if (ImGui::Button("Apply Gravity"))
                    physics_world_.set_gravity(project_.gravity);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Scripts")) {
            if (ImGui::MenuItem("Build & Load Scripts")) {
                compile_and_load_scripts();
            }
            if (ImGui::MenuItem("Load Script DLL...")) {
                std::string default_path = "assets/scripts/game_scripts.dll";
                if (std::filesystem::exists(default_path)) {
                    script_manager_.load_dll(default_path);
                    script_dll_path_ = default_path;
                } else {
                    LOG_WARN("No script DLL found at '%s'. Build your scripts first.", default_path.c_str());
                }
            }
            if (script_manager_.is_loaded()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Loaded: %s",
                    script_manager_.dll_path().c_str());
                if (ImGui::MenuItem("Reload")) script_manager_.reload();
                if (ImGui::MenuItem("Unload")) script_manager_.unload_dll();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open Scripts in Editor")) {
                open_scripts_in_editor();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy",        nullptr, &show_hierarchy_);
            ImGui::MenuItem("Inspector",        nullptr, &show_inspector_);
            ImGui::MenuItem("Viewport",         nullptr, &show_viewport_);
            ImGui::MenuItem("Console",          nullptr, &show_console_);
            ImGui::MenuItem("Assets",           nullptr, &show_assets_);
            ImGui::MenuItem("Script Reference", nullptr, &show_script_ref_);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
                layout_initialized_ = false;
            ImGui::EndMenu();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::Text("FPS: %.0f | %.1f ms", timer_.fps(), timer_.delta() * 1000.0f);

        ImGui::EndMainMenuBar();
    }
}

void EditorApp::render_toolbar() {
    const float toolbar_h = 34.0f;
    float menu_h = ImGui::GetFrameHeight();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + menu_h));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, toolbar_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));

    ImGui::Begin("##ToolbarFixed", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    float btn_h = toolbar_h - 10.0f;
    ImVec2 gizmo_sz(btn_h * 1.6f, btn_h);

    auto gizmo_btn = [&](const char* label, int op) {
        bool active = (gizmo_op_ == op);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.50f, 0.78f, 0.70f));
        if (ImGui::Button(label, gizmo_sz)) gizmo_op_ = op;
        if (active) ImGui::PopStyleColor();
    };

    gizmo_btn("Move", 0);   ImGui::SameLine();
    gizmo_btn("Rotate", 1); ImGui::SameLine();
    gizmo_btn("Scale", 2);

    // Center play/stop buttons
    float center_x = vp->Size.x * 0.5f;
    ImVec2 play_sz(btn_h * 2.4f, btn_h);
    float play_group_w = state_.playing ? (play_sz.x * 2 + 6.0f) : play_sz.x;
    ImGui::SameLine(center_x - play_group_w * 0.5f);

    if (!state_.playing) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.65f, 0.30f, 1.0f));
        if (ImGui::Button("Play", play_sz)) {
            state_.playing = true;
            scene_snapshot_ = SceneSerializer::serialize(scene_);
            game_window_.open(renderer_.context(), renderer_.get_shader_dir(),
                              renderer_.get_meshes(), renderer_.get_materials(),
                              renderer_.get_default_mat());
            physics_world_.sync_from_scene(scene_);
            script_manager_.on_play();
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Stop", play_sz)) {
            state_.playing = false;
            state_.paused  = false;
            script_manager_.on_stop();
            game_window_.close();
            SceneSerializer::deserialize(scene_, scene_snapshot_);
            state_.selected = entt::null;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button(state_.paused ? "Resume" : "Pause", play_sz))
            state_.paused = !state_.paused;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void EditorApp::build_default_layout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID main = dockspace_id;

    ImGuiID left, center_right;
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.15f, &left, &center_right);

    ImGuiID center_bottom, right;
    ImGui::DockBuilderSplitNode(center_right, ImGuiDir_Right, 0.22f, &right, &center_bottom);

    ImGuiID viewport, bottom;
    ImGui::DockBuilderSplitNode(center_bottom, ImGuiDir_Down, 0.28f, &bottom, &viewport);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Viewport", viewport);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Assets", bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}

void EditorApp::run() {
    while (running_ && !window_.should_close()) {
        input_.update();
        window_.poll_events();
        timer_.tick();

        update_editor_camera(timer_.delta());

        // Ctrl+S to save
        if (input_.key_down(GLFW_KEY_LEFT_CONTROL) && input_.key_pressed(GLFW_KEY_S)) {
            if (current_scene_path_.empty()) current_scene_path_ = "assets/scenes/scene.lumios.json";
            save_scene(current_scene_path_);
        }

        // Auto-save
        auto_save_timer_ += timer_.delta();
        if (auto_save_timer_ >= AUTO_SAVE_INTERVAL) {
            auto_save_timer_ = 0.0f;
            std::filesystem::create_directories(".lumios");
            save_scene(".lumios/autosave.json");
        }

        // Auto-compile scripts on source change
        script_check_timer_ += timer_.delta();
        if (script_check_timer_ >= SCRIPT_CHECK_INTERVAL) {
            script_check_timer_ = 0.0f;
            check_script_auto_compile();
        }

        u32 vw = static_cast<u32>(state_.viewport_size.x);
        u32 vh = static_cast<u32>(state_.viewport_size.y);
        if (vw > 0 && vh > 0 && (vw != renderer_.viewport_width() || vh != renderer_.viewport_height()))
            renderer_.resize_viewport(vw, vh);

        if (!renderer_.begin_frame()) continue;

        renderer_.render_scene(scene_, editor_camera_);
        renderer_.render_pick(scene_, editor_camera_);
        renderer_.begin_ui();

        const float toolbar_h = 34.0f;
        ImGuiViewport* main_vp = ImGui::GetMainViewport();
        float menu_h = ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(ImVec2(main_vp->Pos.x, main_vp->Pos.y + menu_h + toolbar_h));
        ImGui::SetNextWindowSize(ImVec2(main_vp->Size.x, main_vp->Size.y - menu_h - toolbar_h));
        ImGui::SetNextWindowViewport(main_vp->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##DockHost", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoDocking);
        ImGui::PopStyleVar(3);
        ImGuiID dockspace_id = ImGui::GetID("LumiosDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0));
        ImGui::End();

        if (!layout_initialized_) {
            build_default_layout(dockspace_id);
            layout_initialized_ = true;
        }

        render_menu_bar();
        render_toolbar();
        state_.gizmo_op = gizmo_op_;
        if (show_hierarchy_) draw_hierarchy_panel(state_);
        if (show_inspector_) draw_inspector_panel(state_);
        if (show_viewport_)  draw_viewport_panel(state_, renderer_.viewport_texture(), &renderer_);
        if (show_console_)   draw_console_panel();
        if (show_assets_)    draw_assets_panel(state_);
        if (show_script_ref_) draw_script_reference_panel();

        renderer_.end_ui();
        renderer_.end_frame();

        if (game_window_.is_open()) {
            if (game_window_.should_close()) {
                state_.playing = false;
                state_.paused  = false;
                script_manager_.on_stop();
                game_window_.close();
                SceneSerializer::deserialize(scene_, scene_snapshot_);
                state_.selected = entt::null;
            } else {
                script_manager_.reload();
                float dt = timer_.delta();
                if (!state_.paused) {
                    physics_world_.step(dt);
                    physics_world_.sync_to_scene(scene_);
                    script_manager_.dispatch_collision_events(physics_world_);
                    script_manager_.late_update(dt);
                }
                game_window_.render_frame(scene_,
                    state_.paused ? nullptr : &script_manager_, dt);
            }
        }
    }
}

void EditorApp::compile_and_load_scripts() {
    LOG_INFO("Compiling scripts...");
    int ret = std::system("cmake --build build --target game_scripts 2>&1");
    if (ret == 0) {
        LOG_INFO("Scripts compiled successfully");
        std::string dll_path = "assets/scripts/game_scripts.dll";
        if (std::filesystem::exists(dll_path)) {
            if (script_manager_.is_loaded()) script_manager_.unload_dll();
            script_manager_.load_dll(dll_path);
            script_dll_path_ = dll_path;
        }
    } else {
        LOG_ERROR("Script compilation failed (exit code %d)", ret);
    }
}

void EditorApp::open_scripts_in_editor() {
    std::string assets_path = (std::filesystem::current_path() / "assets").string();

    std::string cmd;
#ifdef _WIN32
    std::string cursor_cmd = "cursor \"" + assets_path + "\"";
    std::string code_cmd   = "code \"" + assets_path + "\"";
    cmd = "start /B cmd /C \"" + cursor_cmd + " 2>nul || " + code_cmd + "\"";
#else
    cmd = "cursor \"" + assets_path + "\" 2>/dev/null || code \"" + assets_path + "\" &";
#endif
    std::system(cmd.c_str());
    LOG_INFO("Opening assets folder in editor...");
}

void EditorApp::check_script_auto_compile() {
    uint64_t newest = 0;
    try {
        for (auto& entry : std::filesystem::directory_iterator("assets/scripts")) {
            if (entry.path().extension() == ".cpp") {
                auto ftime = std::filesystem::last_write_time(entry);
                auto t = ftime.time_since_epoch().count();
                if (static_cast<uint64_t>(t) > newest)
                    newest = static_cast<uint64_t>(t);
            }
        }
    } catch (...) { return; }

    if (newest == 0) return;

    if (scripts_last_modified_ == 0) {
        scripts_last_modified_ = newest;
        return;
    }

    if (newest != scripts_last_modified_) {
        scripts_last_modified_ = newest;
        LOG_INFO("Script source changed, auto-compiling...");
        compile_and_load_scripts();
    }
}

// --- Project management ---

void EditorApp::save_project(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) { LOG_ERROR("Failed to save project: %s", path.c_str()); return; }
    f << "{\n";
    f << "  \"name\": \"" << project_.name << "\",\n";
    f << "  \"gravity\": [" << project_.gravity.x << "," << project_.gravity.y << "," << project_.gravity.z << "],\n";
    f << "  \"fixed_timestep\": " << project_.fixed_timestep << ",\n";
    f << "  \"bloom\": " << (project_.enable_bloom ? "true" : "false") << ",\n";
    f << "  \"ssao\": " << (project_.enable_ssao ? "true" : "false") << ",\n";
    f << "  \"shadows\": " << (project_.enable_shadows ? "true" : "false") << "\n";
    f << "}\n";
    f.close();
    LOG_INFO("Project saved: %s", path.c_str());
}

void EditorApp::load_project(const std::string& path) {
    if (!std::filesystem::exists(path)) { LOG_ERROR("Project file not found: %s", path.c_str()); return; }
    try {
        std::ifstream f(path);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto j = nlohmann::json::parse(content);
        project_.name = j.value("name", "Untitled");
        if (j.contains("gravity") && j["gravity"].is_array() && j["gravity"].size() >= 3)
            project_.gravity = {j["gravity"][0].get<float>(), j["gravity"][1].get<float>(), j["gravity"][2].get<float>()};
        project_.fixed_timestep = j.value("fixed_timestep", 1.0f / 60.0f);
        project_.enable_bloom   = j.value("bloom", true);
        project_.enable_ssao    = j.value("ssao", true);
        project_.enable_shadows = j.value("shadows", true);
        project_.path = path;
        physics_world_.set_gravity(project_.gravity);
        physics_world_.set_fixed_timestep(project_.fixed_timestep);
        add_recent_project(path);
        LOG_INFO("Project loaded: %s", project_.name.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load project: %s", e.what());
    }
}

void EditorApp::create_new_project(const std::string& folder) {
    namespace fs = std::filesystem;
    try {
        fs::create_directories(folder);
        fs::create_directories(folder + "/assets/scripts");
        fs::create_directories(folder + "/assets/scenes");
        fs::create_directories(folder + "/assets/textures");
        fs::create_directories(folder + "/assets/models");
        fs::create_directories(folder + "/.lumios");

        project_.name = fs::path(folder).filename().string();
        project_.path = folder + "/project.lumios";
        save_project(project_.path);
        add_recent_project(project_.path);
        LOG_INFO("Created new project at %s", folder.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create project: %s", e.what());
    }
}

void EditorApp::load_recent_projects() {
    std::string path;
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home) path = std::string(home) + "/.lumios/recent.json";
    else path = ".lumios/recent.json";

    if (!std::filesystem::exists(path)) return;
    try {
        std::ifstream f(path);
        auto j = nlohmann::json::parse(f);
        if (j.is_array()) {
            for (auto& item : j)
                if (item.is_string()) recent_projects_.push_back(item.get<std::string>());
        }
    } catch (...) {}
}

void EditorApp::save_recent_projects() {
    std::string dir;
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home) dir = std::string(home) + "/.lumios";
    else dir = ".lumios";
    std::filesystem::create_directories(dir);

    std::ofstream f(dir + "/recent.json");
    f << "[\n";
    for (size_t i = 0; i < recent_projects_.size(); i++) {
        f << "  \"" << recent_projects_[i] << "\"";
        if (i + 1 < recent_projects_.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
}

void EditorApp::add_recent_project(const std::string& path) {
    recent_projects_.erase(
        std::remove(recent_projects_.begin(), recent_projects_.end(), path),
        recent_projects_.end());
    recent_projects_.insert(recent_projects_.begin(), path);
    if (recent_projects_.size() > 5)
        recent_projects_.resize(5);
    save_recent_projects();
}

void EditorApp::shutdown() {
    script_manager_.shutdown();
    physics_world_.shutdown();
    if (game_window_.is_open()) game_window_.close();
    renderer_.shutdown();
    window_.shutdown();
    LOG_INFO("Editor shut down");
}

} // namespace lumios::editor
