#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class FpsController : public lumios::LumiosScript {
    float move_speed_ = 5.0f;
    float sprint_mult_ = 2.5f;
    float mouse_sens_ = 0.15f;
    float jump_force_ = 5.0f;
    float yaw_ = 0.0f, pitch_ = 0.0f;
    glm::vec3 velocity_{0.0f};

    LUMIOS_PROPERTIES_BEGIN(FpsController)
        LUMIOS_PROP_FLOAT(move_speed_, 0.0f, 50.0f)
        LUMIOS_PROP_FLOAT(sprint_mult_, 1.0f, 10.0f)
        LUMIOS_PROP_FLOAT(mouse_sens_, 0.01f, 2.0f)
        LUMIOS_PROP_FLOAT(jump_force_, 0.0f, 30.0f)
    LUMIOS_PROPERTIES_END()

public:
    void on_start(lumios::ScriptContext& ctx) override {
        auto rot = ctx.rotation();
        yaw_ = rot.y;
        pitch_ = rot.x;
    }

    void on_update(lumios::ScriptContext& ctx) override {
        float dt = ctx.dt();
        float speed = move_speed_;
        if (ctx.key_down(GLFW_KEY_LEFT_SHIFT)) speed *= sprint_mult_;

        if (ctx.mouse_down(GLFW_MOUSE_BUTTON_RIGHT)) {
            yaw_ += static_cast<float>(ctx.mouse_dx()) * mouse_sens_;
            pitch_ -= static_cast<float>(ctx.mouse_dy()) * mouse_sens_;
            pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
        }

        glm::vec3 forward = ctx.get_forward();
        glm::vec3 right = ctx.get_right();

        glm::vec3 move{0.0f};
        if (ctx.key_down(GLFW_KEY_W)) move += forward;
        if (ctx.key_down(GLFW_KEY_S)) move -= forward;
        if (ctx.key_down(GLFW_KEY_A)) move -= right;
        if (ctx.key_down(GLFW_KEY_D)) move += right;
        if (glm::length(move) > 0.001f) move = glm::normalize(move);

        auto pos = ctx.position();
        pos += move * speed * dt;

        if (ctx.key_pressed(GLFW_KEY_SPACE))
            velocity_.y = jump_force_;

        velocity_.y -= 9.81f * dt;
        pos += velocity_ * dt;

        if (pos.y < 0.0f) {
            pos.y = 0.0f;
            velocity_.y = 0.0f;
        }

        ctx.set_position(pos);
        ctx.set_rotation(glm::vec3(pitch_, yaw_, 0.0f));
    }
};

LUMIOS_REGISTER_SCRIPT(FpsController)
