#include <lumios.h>
#include <GLFW/glfw3.h>

class DemoApp : public lumios::Application {
    lumios::Engine* engine_ = nullptr;
    lumios::Camera  camera_;

    lumios::MeshHandle     cube_mesh_, sphere_mesh_, plane_mesh_;
    lumios::MaterialHandle white_mat_, red_mat_, blue_mat_;

    float time_ = 0.0f;

public:
    void bind(lumios::Engine& e) { engine_ = &e; }

    void on_init() override {
        auto& r = engine_->renderer();

        cube_mesh_   = r.upload_mesh(lumios::assets::create_cube());
        sphere_mesh_ = r.upload_mesh(lumios::assets::create_sphere(32, 16, 0.5f));
        plane_mesh_  = r.upload_mesh(lumios::assets::create_plane(30.0f, 4));

        white_mat_ = r.create_material({.base_color = {0.9f, 0.9f, 0.9f, 1.0f}, .roughness = 0.8f});
        red_mat_   = r.create_material({.base_color = {0.9f, 0.15f, 0.1f, 1.0f}, .metallic = 0.3f, .roughness = 0.4f});
        blue_mat_  = r.create_material({.base_color = {0.1f, 0.3f, 0.9f, 1.0f}, .metallic = 0.7f, .roughness = 0.2f});

        auto& scene = engine_->scene();

        // Ground plane
        auto ground = scene.create_entity("ground");
        scene.get<lumios::Transform>(ground).position = {0, -1, 0};
        scene.add<lumios::MeshComponent>(ground, cube_mesh_, white_mat_);
        scene.get<lumios::Transform>(ground).scale = {30, 0.1f, 30};

        // Center sphere
        auto sphere = scene.create_entity("sphere");
        scene.get<lumios::Transform>(sphere).position = {0, 0.5f, 0};
        scene.add<lumios::MeshComponent>(sphere, sphere_mesh_, blue_mat_);

        // Surrounding cubes
        for (int i = 0; i < 6; i++) {
            float angle = static_cast<float>(i) / 6.0f * 2.0f * lumios::PI;
            float x = cos(angle) * 4.0f;
            float z = sin(angle) * 4.0f;

            auto cube = scene.create_entity("cube_" + std::to_string(i));
            scene.get<lumios::Transform>(cube).position = {x, 0.0f, z};
            scene.get<lumios::Transform>(cube).scale = {0.8f, 0.8f, 0.8f};
            scene.add<lumios::MeshComponent>(cube, cube_mesh_, (i % 2 == 0) ? red_mat_ : blue_mat_);
        }

        // Directional light (sun)
        auto sun = scene.create_entity("sun");
        scene.get<lumios::Transform>(sun).rotation = {-45.0f, 30.0f, 0.0f};
        scene.add<lumios::LightComponent>(sun,
            lumios::LightType::Directional, glm::vec3(1.0f, 0.95f, 0.85f), 1.2f);

        // Point lights
        auto light1 = scene.create_entity("light_red");
        scene.get<lumios::Transform>(light1).position = {3, 3, 3};
        scene.add<lumios::LightComponent>(light1,
            lumios::LightType::Point, glm::vec3(1.0f, 0.3f, 0.2f), 2.0f, 15.0f);

        auto light2 = scene.create_entity("light_blue");
        scene.get<lumios::Transform>(light2).position = {-3, 2, -3};
        scene.add<lumios::LightComponent>(light2,
            lumios::LightType::Point, glm::vec3(0.2f, 0.4f, 1.0f), 2.0f, 15.0f);

        // Camera
        camera_.set_perspective(60.0f, engine_->window().aspect(), 0.1f, 500.0f);
        camera_.set_position({0, 4, 10});
        camera_.look_at({0, 0, 0});
    }

    void on_update(float dt) override {
        time_ += dt;
        auto& input = engine_->input();

        // ESC to close
        if (input.key_pressed(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(engine_->window().handle(), GLFW_TRUE);

        // Camera movement
        float speed = 8.0f * dt;
        if (input.key_down(GLFW_KEY_LEFT_SHIFT)) speed *= 3.0f;
        if (input.key_down(GLFW_KEY_W)) camera_.move_forward(speed);
        if (input.key_down(GLFW_KEY_S)) camera_.move_forward(-speed);
        if (input.key_down(GLFW_KEY_A)) camera_.move_right(-speed);
        if (input.key_down(GLFW_KEY_D)) camera_.move_right(speed);
        if (input.key_down(GLFW_KEY_SPACE))      camera_.move_up(speed);
        if (input.key_down(GLFW_KEY_LEFT_CONTROL)) camera_.move_up(-speed);

        // Mouse look with right button
        if (input.mouse_down(GLFW_MOUSE_BUTTON_RIGHT)) {
            camera_.rotate(
                static_cast<float>(input.mouse_dx()) * 0.15f,
                static_cast<float>(input.mouse_dy()) * 0.15f
            );
        }

        camera_.set_aspect(engine_->window().aspect());

        // Animate cubes
        auto cube_view = engine_->scene().view<lumios::Transform, lumios::MeshComponent, lumios::NameComponent>();
        for (auto entity : cube_view) {
            auto& name = cube_view.get<lumios::NameComponent>(entity);
            if (name.name.starts_with("cube_")) {
                auto& t = cube_view.get<lumios::Transform>(entity);
                t.rotation.y = time_ * 45.0f;
                t.position.y = sin(time_ * 2.0f + t.position.x) * 0.5f + 0.5f;
            }
        }
    }

    void on_render() override {
        engine_->renderer().render_scene(engine_->scene(), camera_);
    }
};

int main() {
    DemoApp app;

    lumios::EngineConfig config;
    config.window.title  = "Lumios Engine - Demo";
    config.window.width  = 1600;
    config.window.height = 900;
    config.shader_dir    = LUMIOS_SHADER_DIR;

    lumios::Engine engine;
    app.bind(engine);

    if (!engine.init(config, app)) return 1;
    engine.run();
    engine.shutdown();

    return 0;
}
