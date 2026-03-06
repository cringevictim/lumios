#pragma once

#include "defines.h"
#include "core/types.h"
#include "core/log.h"
#include "core/timer.h"
#include "core/event.h"
#include "core/input.h"
#include "math/math.h"
#include "platform/window.h"
#include "graphics/gpu_types.h"
#include "graphics/camera.h"
#include "graphics/renderer.h"
#include "scene/scene.h"
#include "scene/components.h"
#include "assets/loader.h"

namespace lumios {

struct EngineConfig {
    WindowConfig window;
    std::string  shader_dir;
};

class Application {
public:
    virtual ~Application() = default;
    virtual void on_init()   {}
    virtual void on_update(float dt) { (void)dt; }
    virtual void on_render() {}
    virtual void on_shutdown() {}
};

class LUMIOS_API Engine {
    Unique<Renderer> renderer_;
    Window           window_;
    Input            input_;
    Timer            timer_;
    EventBus         events_;
    Scene            scene_;
    Application*     app_ = nullptr;
    bool             running_ = false;

public:
    bool init(const EngineConfig& config, Application& app);
    void run();
    void shutdown();

    Renderer& renderer() { return *renderer_; }
    Window&   window()   { return window_; }
    Input&    input()    { return input_; }
    Timer&    timer()    { return timer_; }
    EventBus& events()   { return events_; }
    Scene&    scene()    { return scene_; }
};

} // namespace lumios
