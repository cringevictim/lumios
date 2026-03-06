#pragma once

#include "../defines.h"
#include "../core/event.h"
#include <string>
#include <cstdint>

struct GLFWwindow;

namespace lumios {

struct WindowConfig {
    std::string title = "Lumios Engine";
    uint32_t width  = 1280;
    uint32_t height = 720;
    bool resizable  = true;
    bool vsync      = true;
};

class LUMIOS_API Window {
    GLFWwindow* handle_ = nullptr;
    uint32_t width_, height_;
    bool framebuffer_resized_ = false;
    EventBus* events_ = nullptr;

public:
    bool init(const WindowConfig& config, EventBus& events);
    void shutdown();
    void poll_events();
    bool should_close() const;
    void get_size(int& w, int& h) const;
    void get_framebuffer_size(int& w, int& h) const;

    GLFWwindow* handle() const { return handle_; }
    uint32_t    width()  const { return width_; }
    uint32_t    height() const { return height_; }
    float       aspect() const { return height_ > 0 ? static_cast<float>(width_) / height_ : 1.0f; }

    bool framebuffer_resized() const { return framebuffer_resized_; }
    void reset_resize_flag() { framebuffer_resized_ = false; }

    void on_resize(uint32_t w, uint32_t h);
};

} // namespace lumios
