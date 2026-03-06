#pragma once

#include "../math/math.h"

namespace lumios {

class Camera {
    glm::vec3 position_{0, 0, 5};
    float     yaw_   = -90.0f;
    float     pitch_ = 0.0f;

    glm::vec3 front_{0, 0, -1};
    glm::vec3 up_{0, 1, 0};
    glm::vec3 right_{1, 0, 0};

    float fov_  = 60.0f;
    float near_ = 0.1f;
    float far_  = 1000.0f;
    float aspect_ = 16.0f / 9.0f;

    void update_vectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        f.y = sin(glm::radians(pitch_));
        f.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_ = glm::normalize(f);
        right_ = glm::normalize(glm::cross(front_, glm::vec3(0, 1, 0)));
        up_    = glm::normalize(glm::cross(right_, front_));
    }

public:
    Camera() { update_vectors(); }

    void set_perspective(float fov, float aspect, float near_p, float far_p) {
        fov_ = fov; aspect_ = aspect; near_ = near_p; far_ = far_p;
    }

    void set_position(const glm::vec3& pos) { position_ = pos; }
    void set_aspect(float a) { aspect_ = a; }

    void look_at(const glm::vec3& target) {
        glm::vec3 dir = glm::normalize(target - position_);
        pitch_ = glm::degrees(asin(dir.y));
        yaw_   = glm::degrees(atan2(dir.z, dir.x));
        update_vectors();
    }

    void rotate(float yaw_delta, float pitch_delta) {
        yaw_   += yaw_delta;
        pitch_ -= pitch_delta;
        pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
        update_vectors();
    }

    void move_forward(float amount) { position_ += front_ * amount; }
    void move_right(float amount)   { position_ += right_ * amount; }
    void move_up(float amount)      { position_ += up_ * amount; }

    glm::mat4 view()       const { return glm::lookAt(position_, position_ + front_, up_); }
    glm::mat4 projection() const { return glm::perspective(glm::radians(fov_), aspect_, near_, far_); }

    const glm::vec3& position() const { return position_; }
    const glm::vec3& front()    const { return front_; }
    float fov()    const { return fov_; }
    float aspect() const { return aspect_; }
};

} // namespace lumios
