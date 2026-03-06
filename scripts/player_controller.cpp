#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class PlayerController : public lumios::LumiosScript {
    float move_speed_ = 5.0f;
    float look_speed_ = 2.0f;

public:
    void on_update(lumios::ScriptContext& ctx) override {
        float dt = ctx.delta_time;
        auto pos = ctx.position();
        auto rot = ctx.rotation();

        float yaw_rad = glm::radians(rot.y);
        glm::vec3 forward(cos(yaw_rad), 0.0f, sin(yaw_rad));
        glm::vec3 right(-sin(yaw_rad), 0.0f, cos(yaw_rad));

        if (ctx.key_down(GLFW_KEY_UP))    pos += forward * move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_DOWN))  pos -= forward * move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_LEFT))  pos -= right * move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_RIGHT)) pos += right * move_speed_ * dt;

        if (ctx.key_down(GLFW_KEY_PAGE_UP))   pos.y += move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_PAGE_DOWN)) pos.y -= move_speed_ * dt;

        if (ctx.key_down(GLFW_KEY_Q)) rot.y -= look_speed_ * 60.0f * dt;
        if (ctx.key_down(GLFW_KEY_E)) rot.y += look_speed_ * 60.0f * dt;

        ctx.set_position(pos);
        ctx.set_rotation(rot);
    }
};

LUMIOS_REGISTER_SCRIPT(PlayerController)
