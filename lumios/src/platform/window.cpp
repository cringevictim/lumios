#include "window.h"
#include "../core/log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace lumios {

static bool s_glfw_initialized = false;

bool Window::init(const WindowConfig& config, EventBus& events) {
    events_ = &events;
    width_ = config.width;
    height_ = config.height;

    if (!s_glfw_initialized) {
        if (!glfwInit()) {
            LOG_FATAL("Failed to initialize GLFW");
            return false;
        }
        s_glfw_initialized = true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    handle_ = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (!handle_) {
        LOG_FATAL("Failed to create GLFW window");
        return false;
    }

    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, [](GLFWwindow* win, int w, int h) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        self->on_resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    });

    LOG_INFO("Window created: %ux%u", config.width, config.height);
    return true;
}

void Window::shutdown() {
    if (handle_) {
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
    }
    glfwTerminate();
    s_glfw_initialized = false;
    LOG_INFO("Window destroyed");
}

void Window::poll_events() {
    glfwPollEvents();
}

bool Window::should_close() const {
    return glfwWindowShouldClose(handle_);
}

void Window::get_size(int& w, int& h) const {
    glfwGetWindowSize(handle_, &w, &h);
}

void Window::get_framebuffer_size(int& w, int& h) const {
    glfwGetFramebufferSize(handle_, &w, &h);
}

void Window::on_resize(uint32_t w, uint32_t h) {
    width_ = w;
    height_ = h;
    framebuffer_resized_ = true;
    if (events_) events_->emit(WindowResizeEvent{w, h});
}

} // namespace lumios
