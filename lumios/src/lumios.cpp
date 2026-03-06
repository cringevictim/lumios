#include "lumios.h"

namespace lumios {

bool Engine::init(const EngineConfig& config, Application& app) {
    app_ = &app;
    log::init();
    LOG_INFO("Lumios Engine v%d.%d.%d", LUMIOS_VERSION_MAJOR, LUMIOS_VERSION_MINOR, LUMIOS_VERSION_PATCH);

    if (!window_.init(config.window, events_)) {
        LOG_FATAL("Window initialization failed");
        return false;
    }

    input_.init(window_.handle());
    timer_.reset();

    renderer_ = Renderer::create();
    if (!renderer_->init(window_, config.shader_dir)) {
        LOG_FATAL("Renderer initialization failed");
        return false;
    }

    app_->on_init();
    running_ = true;
    LOG_INFO("Engine initialized successfully");
    return true;
}

void Engine::run() {
    while (running_ && !window_.should_close()) {
        window_.poll_events();
        input_.update();
        timer_.tick();

        app_->on_update(timer_.delta());

        if (renderer_->begin_frame()) {
            app_->on_render();
            renderer_->end_frame();
        }
    }
}

void Engine::shutdown() {
    app_->on_shutdown();
    renderer_->shutdown();
    window_.shutdown();
    LOG_INFO("Engine shut down");
}

} // namespace lumios
