#include "input.h"
#include <GLFW/glfw3.h>

namespace lumios {

static Input* s_instance = nullptr;

void Input::init(GLFWwindow* window) {
    s_instance = this;
    keys_.fill(false);
    prev_keys_.fill(false);
    mouse_buttons_.fill(false);
    prev_mouse_buttons_.fill(false);

    glfwSetKeyCallback(window, [](GLFWwindow*, int key, int, int action, int) {
        if (s_instance && key >= 0 && key < 512) s_instance->on_key(key, action);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow*, int button, int action, int) {
        if (s_instance && button >= 0 && button < 8) s_instance->on_mouse_button(button, action);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow*, double x, double y) {
        if (s_instance) s_instance->on_mouse_move(x, y);
    });
    glfwSetScrollCallback(window, [](GLFWwindow*, double x, double y) {
        if (s_instance) s_instance->on_scroll(x, y);
    });
}

void Input::update() {
    prev_keys_ = keys_;
    prev_mouse_buttons_ = mouse_buttons_;
    prev_mouse_x_ = mouse_x_;
    prev_mouse_y_ = mouse_y_;
    scroll_x_ = 0;
    scroll_y_ = 0;
}

void Input::on_key(int key, int action) {
    if (action == GLFW_PRESS)   keys_[key] = true;
    if (action == GLFW_RELEASE) keys_[key] = false;
}

void Input::on_mouse_button(int button, int action) {
    if (action == GLFW_PRESS)   mouse_buttons_[button] = true;
    if (action == GLFW_RELEASE) mouse_buttons_[button] = false;
}

void Input::on_mouse_move(double x, double y) {
    if (first_mouse_) {
        prev_mouse_x_ = x;
        prev_mouse_y_ = y;
        first_mouse_ = false;
    }
    mouse_x_ = x;
    mouse_y_ = y;
}

void Input::on_scroll(double xoff, double yoff) {
    scroll_x_ = xoff;
    scroll_y_ = yoff;
}

} // namespace lumios
