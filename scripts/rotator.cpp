#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class Rotator : public lumios::LumiosScript {
    float speed_ = 90.0f;

public:
    void on_create(lumios::ScriptContext& ctx) override {
        speed_ = 90.0f;
    }

    void on_update(lumios::ScriptContext& ctx) override {
        auto rot = ctx.rotation();
        rot.y += speed_ * ctx.delta_time;
        if (rot.y > 360.0f) rot.y -= 360.0f;
        ctx.set_rotation(rot);
    }
};

LUMIOS_REGISTER_SCRIPT(Rotator)
