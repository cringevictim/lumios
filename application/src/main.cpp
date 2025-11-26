#include <lumios.h>
#include <iostream>

// Example game application using the Lumios Engine
class MyGameApplication : public lumios::Application {
public:
    bool Initialize() override {
        LOG_INFO("MyGameApplication: Initializing...");
        
        // Initialize your game-specific resources here
        // For example:
        // - Load textures
        // - Load models
        // - Setup scenes
        // - Initialize game state
        
        LOG_INFO("MyGameApplication: Initialization complete");
        return true;
    }

    void Update(float deltaTime) override {
        // Update game logic here
        // For example:
        // - Update player movement
        // - Update AI
        // - Update physics
        // - Handle collisions
        // - Update animations
        
        // Example: Log FPS every second
        static float timeAccumulator = 0.0f;
        timeAccumulator += deltaTime;
        /*if (timeAccumulator >= 1.0f) {
            if (m_Engine) {
                auto* time = m_Engine->GetTime();
                if (time) {
                    LOG_INFO("FPS: {:.1f}, Frame: {}", time->GetFPS(), time->GetFrameCount());
                }
            }
            timeAccumulator = 0.0f;
        }*/
    }

    void Render() override {
        // Render your game here
        // For example:
        // - Clear screen
        // - Draw background
        // - Draw game objects
        // - Draw UI
        // - Present frame
        
        // The graphics frontend handles the basic rendering pipeline
        // You can access the graphics system through m_Engine->GetGraphics()
    }

    void Shutdown() override {
        LOG_INFO("MyGameApplication: Shutting down...");
        
        // Cleanup your game-specific resources here
        // For example:
        // - Unload textures
        // - Unload models
        // - Save game state
        // - Cleanup game objects
        
        LOG_INFO("MyGameApplication: Shutdown complete");
    }

    // Event handling examples
    void OnWindowResize(int width, int height) override {
        LOG_INFO("Window resized to {}x{}", width, height);
    }

    void OnKeyPressed(int key) override {
        LOG_DEBUG("Key pressed: {}", key);
        
        // Example: Exit on Escape key
        //if (key == 256) { // GLFW_KEY_ESCAPE
        //    if (m_Engine) {
        //        m_Engine->RequestShutdown();
        //    }
        //}
    }

    void OnKeyReleased(int key) override {
        LOG_DEBUG("Key released: {}", key);
    }

    void OnMousePressed(int button) override {
        LOG_DEBUG("Mouse button pressed: {}", button);
    }

    void OnMouseMoved(float x, float y) override {
        // Handle mouse movement (usually too frequent to log)
    }
};

int main() {
    LOG_INFO("Starting Lumios Engine Application");

    // Configure the engine
    lumios::EngineConfig config;
    config.enable_graphics = true;
    config.enable_audio = true;
    config.enable_physics = true;
    config.log_level = lumios::utils::LogLevel::LUM_INFO;
    config.enable_log_colors = true;

    // Initialize the engine
    if (!lumios::InitializeEngine(config)) {
        LOG_ERROR("Failed to initialize Lumios Engine");
        return -1;
    }

    // Create and run your application
    MyGameApplication app;
    lumios::RunApplication(&app);

    // Shutdown the engine
    lumios::ShutdownEngine();

    LOG_INFO("Application ended successfully");
    return 0;
}

// Alternative main function using the engine directly (more control)
/*
int main() {
    LOG_INFO("Starting Lumios Engine Application (Direct Engine Usage)");

    // Create engine instance
    lumios::Engine engine;
    
    // Configure and initialize
    lumios::EngineConfig config;
    config.enable_graphics = true;
    config.log_level = lumios::utils::LogLevel::DEBUG;
    
    if (!engine.Initialize(config)) {
        LOG_ERROR("Failed to initialize engine");
        return -1;
    }

    // Create application
    MyGameApplication app;
    
    // Run the application
    engine.Run(&app);
    
    // Engine automatically shuts down when destroyed
    LOG_INFO("Application ended successfully");
    return 0;
}
*/

// Legacy main function (for backward compatibility)
/*
int main() {
    // Old way - still works but deprecated
    lumios::initialize();
    lumios::run();
    return 0;
}
*/
