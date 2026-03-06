#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class PlayerController : public lumios::LumiosScript {
    float move_speed_ = 5.0f;
    float look_speed_ = 2.0f;

    LUMIOS_PROPERTIES_BEGIN(PlayerController)
        LUMIOS_PROP_FLOAT(move_speed_, 0.0f, 50.0f)
        LUMIOS_PROP_FLOAT(look_speed_, 0.0f, 10.0f)
    LUMIOS_PROPERTIES_END()

public:
    void on_update(lumios::ScriptContext& ctx) override {
        float dt = ctx.dt();
        auto pos = ctx.position();

        glm::vec3 forward = ctx.get_forward();
        glm::vec3 right = ctx.get_right();

        if (ctx.key_down(GLFW_KEY_UP))    pos += forward * move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_DOWN))  pos -= forward * move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_LEFT))  pos -= right * move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_RIGHT)) pos += right * move_speed_ * dt;

        if (ctx.key_down(GLFW_KEY_PAGE_UP))   pos.y += move_speed_ * dt;
        if (ctx.key_down(GLFW_KEY_PAGE_DOWN)) pos.y -= move_speed_ * dt;

        auto rot = ctx.rotation();
        if (ctx.key_down(GLFW_KEY_Q)) rot.y -= look_speed_ * 60.0f * dt;
        if (ctx.key_down(GLFW_KEY_E)) rot.y += look_speed_ * 60.0f * dt;

        ctx.set_position(pos);
        ctx.set_rotation(rot);
    }
};

LUMIOS_REGISTER_SCRIPT(PlayerController)
