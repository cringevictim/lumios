#pragma once

#include "editor_renderer.h"
#include "editor_panels.h"
#include "game_window.h"
#include "platform/window.h"
#include "core/input.h"
#include "core/timer.h"
#include "core/event.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scripting/script_manager.h"
#include "physics/physics_world.h"
#include "graphics/camera.h"

namespace lumios::editor {

struct ProjectConfig {
    std::string name = "Untitled";
    std::string path;
    glm::vec3   gravity{0, -9.81f, 0};
    float       fixed_timestep = 1.0f / 60.0f;
    bool        enable_bloom   = true;
    bool        enable_ssao    = true;
    bool        enable_shadows = true;
};

class EditorApp {
    Window          window_;
    EventBus        events_;
    Input           input_;
    Timer           timer_;
    EditorRenderer  renderer_;
    Scene           scene_;
    Camera          editor_camera_;
    EditorState     state_;
    GameWindow      game_window_;
    ScriptManager   script_manager_;
    PhysicsWorld    physics_world_;
    ProjectConfig   project_;
    std::string     scene_snapshot_;

    glm::vec3 focus_point_{0, 0, 0};
    float     orbit_distance_ = 12.0f;
    float     orbit_yaw_   = -45.0f;
    float     orbit_pitch_ = -30.0f;

    bool running_ = false;
    bool layout_initialized_ = false;
    bool show_hierarchy_ = true;
    bool show_inspector_ = true;
    bool show_viewport_  = true;
    bool show_console_   = true;
    bool show_assets_    = true;
    bool show_script_ref_ = false;

    int gizmo_op_ = 0;
    bool viewport_captured_ = false;

    std::string current_scene_path_;
    std::string script_dll_path_;
    float auto_save_timer_ = 0.0f;
    float script_check_timer_ = 0.0f;
    uint64_t scripts_last_modified_ = 0;
    static constexpr float AUTO_SAVE_INTERVAL = 60.0f;
    static constexpr float SCRIPT_CHECK_INTERVAL = 1.0f;

    std::vector<std::string> recent_projects_;

    void setup_default_scene();
    void update_editor_camera(float dt);
    void update_orbit_camera();
    void render_menu_bar();
    void render_toolbar();
    void build_default_layout(ImGuiID dockspace_id);
    void save_scene(const std::string& path);
    void load_scene(const std::string& path);
    void compile_and_load_scripts();
    void open_scripts_in_editor();
    void check_script_auto_compile();

    void save_project(const std::string& path);
    void load_project(const std::string& path);
    void create_new_project(const std::string& folder);
    void load_recent_projects();
    void save_recent_projects();
    void add_recent_project(const std::string& path);

public:
    bool init();
    void run();
    void shutdown();
};

} // namespace lumios::editor
