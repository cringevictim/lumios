#pragma once

#include "imgui.h"
#include "scene/scene.h"
#include "scene/components.h"
#include "graphics/camera.h"
#include "core/log.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <deque>

namespace lumios {
class EditorRenderer;
class ScriptManager;
}

namespace lumios::editor {

struct EditorState {
    Scene*    scene  = nullptr;
    Camera*   camera = nullptr;
    entt::entity selected = entt::null;
    bool playing = false;
    bool paused  = false;

    // Viewport
    ImVec2 viewport_size{800, 600};
    bool   viewport_hovered = false;
    bool   viewport_focused = false;

    // Gizmo: 0=translate, 1=rotate, 2=scale
    int gizmo_op = 0;

    // Mesh primitives available
    MeshHandle     cube_mesh, sphere_mesh, plane_mesh;
    MaterialHandle default_mat;

    // Script property access
    ScriptManager* script_manager = nullptr;

    // Assets panel state
    std::string assets_root = "assets";
    std::string current_assets_path = "assets";
};

void draw_hierarchy_panel(EditorState& state);
void draw_inspector_panel(EditorState& state);
void draw_viewport_panel(EditorState& state, ImTextureID scene_texture, lumios::EditorRenderer* renderer = nullptr);
void draw_console_panel();
void draw_assets_panel(EditorState& state);
void draw_script_reference_panel();

void init_console_log();

} // namespace lumios::editor
