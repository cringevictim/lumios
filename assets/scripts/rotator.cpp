#include "scripting/lumios_api.h"
#include <GLFW/glfw3.h>

class Rotator : public lumios::LumiosScript {
    float speed_ = 90.0f;
    glm::vec3 axis_{0.0f, 1.0f, 0.0f};

    LUMIOS_PROPERTIES_BEGIN(Rotator)
        LUMIOS_PROP_FLOAT(speed_, -720.0f, 720.0f)
        LUMIOS_PROP_VEC3(axis_)
    LUMIOS_PROPERTIES_END()

public:
    void on_update(lumios::ScriptContext& ctx) override {
        auto rot = ctx.rotation();
        rot += axis_ * speed_ * ctx.dt();
        ctx.set_rotation(rot);
    }
};

LUMIOS_REGISTER_SCRIPT(Rotator)
