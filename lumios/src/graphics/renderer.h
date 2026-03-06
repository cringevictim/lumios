#pragma once

#include "../core/types.h"
#include "gpu_types.h"
#include "camera.h"
#include <string>

namespace lumios {

class Window;
class Scene;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(Window& window, const std::string& shader_dir) = 0;
    virtual void shutdown() = 0;

    virtual bool begin_frame() = 0;
    virtual void end_frame() = 0;

    virtual MeshHandle     upload_mesh(const MeshData& data) = 0;
    virtual TextureHandle  load_texture(const std::string& path) = 0;
    virtual MaterialHandle create_material(const MaterialData& data) = 0;

    virtual void render_scene(Scene& scene, const Camera& camera) = 0;

    static Unique<Renderer> create();
};

} // namespace lumios
