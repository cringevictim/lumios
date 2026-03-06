#pragma once

#include "../defines.h"
#include <array>

struct GLFWwindow;

namespace lumios {

class LUMIOS_API Input {
    std::array<bool, 512> keys_{};
    std::array<bool, 512> prev_keys_{};
    std::array<bool, 8>   mouse_buttons_{};
    std::array<bool, 8>   prev_mouse_buttons_{};
    double mouse_x_ = 0, mouse_y_ = 0;
    double prev_mouse_x_ = 0, prev_mouse_y_ = 0;
    double scroll_x_ = 0, scroll_y_ = 0;
    bool first_mouse_ = true;

public:
    void init(GLFWwindow* window);
    void update();

    bool key_down(int key)      const { return keys_[key]; }
    bool key_pressed(int key)   const { return keys_[key] && !prev_keys_[key]; }
    bool key_released(int key)  const { return !keys_[key] && prev_keys_[key]; }

    bool mouse_down(int btn)    const { return mouse_buttons_[btn]; }
    bool mouse_pressed(int btn) const { return mouse_buttons_[btn] && !prev_mouse_buttons_[btn]; }

    double mouse_x() const { return mouse_x_; }
    double mouse_y() const { return mouse_y_; }
    double mouse_dx() const { return mouse_x_ - prev_mouse_x_; }
    double mouse_dy() const { return mouse_y_ - prev_mouse_y_; }
    double scroll_x() const { return scroll_x_; }
    double scroll_y() const { return scroll_y_; }

    void on_key(int key, int action);
    void on_mouse_button(int button, int action);
    void on_mouse_move(double x, double y);
    void on_scroll(double xoff, double yoff);
};

} // namespace lumios
