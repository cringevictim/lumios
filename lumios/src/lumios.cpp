#include "lumios.h"

#include "graphics/frontend/graphics_frontend.h"
#include "graphics/config/graphics_config.h"

#include <iostream>

namespace lumios {

    // Global engine instance
    static Engine* g_engine_instance = nullptr;

    // Engine implementation
    Engine::Engine() = default;

    Engine::~Engine() {
        Shutdown();
    }

    bool Engine::Initialize(const EngineConfig& config) {
        if (m_IsInitialized) {
            LOG_WARN("Engine already initialized");
            return true;
        }

        m_Config = config;
        
        // Setup logging
        utils::set_log_level(config.log_level);
        utils::enable_colors(config.enable_log_colors);
        
        LOG_INFO("Initializing Lumios Engine...");
        
        if (!InitializeSystems()) {
            LOG_ERROR("Failed to initialize engine systems");
            return false;
        }

        m_IsInitialized = true;
        LOG_INFO("Lumios Engine initialized successfully");
        return true;
    }

    void Engine::Run(Application* app) {
        if (!m_IsInitialized) {
            LOG_ERROR("Engine not initialized");
            return;
        }

        if (!app) {
            LOG_ERROR("No application provided");
            return;
        }

        m_CurrentApp = app;
        app->m_Engine = this;

        LOG_INFO("Starting application...");
        
        if (!app->Initialize()) {
            LOG_ERROR("Application initialization failed");
            return;
        }

        m_IsRunning = true;
        
        // If graphics frontend is available, let it control the main loop
        if (m_Graphics) {
            m_Graphics->SetUpdateCallback([this, app](float deltaTime) {
                if (m_Time) m_Time->Update(deltaTime);
                UpdateSystems(deltaTime);
                app->Update(deltaTime);
            });

            m_Graphics->SetRenderCallback([this, app]() {
                RenderSystems();
                app->Render();
            });

            m_Graphics->Run();
        } else {
            // Fallback main loop without graphics
            while (m_IsRunning && !ShouldClose()) {
                float deltaTime = 0.016f; // Default to ~60 FPS
                if (m_Time) m_Time->Update(deltaTime);
                UpdateSystems(deltaTime);
                app->Update(deltaTime);
                RenderSystems();
                app->Render();
            }
        }

        // Cleanup
        app->Shutdown();
        m_IsRunning = false;
        LOG_INFO("Application ended");
    }

    void Engine::Shutdown() {
        if (!m_IsInitialized) return;

        LOG_INFO("Shutting down Lumios Engine...");
        
        ShutdownSystems();
        
        m_IsInitialized = false;
        m_IsRunning = false;
        m_CurrentApp = nullptr;
        
        LOG_INFO("Lumios Engine shutdown complete");
    }

    bool Engine::ShouldClose() const {
        if (m_ShouldShutdown) return true;
        if (m_Graphics && m_Graphics->ShouldClose()) return true;
        return false;
    }

    bool Engine::InitializeSystems() {
        LOG_INFO("Initializing engine systems...");

        // Initialize Time system
        m_Time = std::make_unique<Time>();
        LOG_INFO("Time system initialized");

        // Initialize Graphics system
        if (m_Config.enable_graphics) {
            graphics::GraphicsConfig graphicsConfig;
            // Configure graphics based on engine config
            // graphicsConfig.SetValidationEnabled(m_Config.log_level <= utils::LogLevel::DEBUG);
            
            m_Graphics = std::make_unique<graphics::GraphicsFrontend>(graphicsConfig);
            
            if (!m_Graphics->Initialize()) {
                LOG_ERROR("Failed to initialize graphics system");
                return false;
            }
            LOG_INFO("Graphics system initialized");
        }

        // Initialize Input system
        m_Input = std::make_unique<Input>();
        LOG_INFO("Input system initialized");

        // Initialize Resources system
        m_Resources = std::make_unique<Resources>();
        LOG_INFO("Resources system initialized");

        // Initialize Audio system
        if (m_Config.enable_audio) {
            m_Audio = std::make_unique<Audio>();
            LOG_INFO("Audio system initialized");
        }

        // Initialize Physics system
        if (m_Config.enable_physics) {
            m_Physics = std::make_unique<Physics>();
            LOG_INFO("Physics system initialized");
        }

        return true;
    }

    void Engine::UpdateSystems(float deltaTime) {
        if (m_Input) m_Input->Update();
        if (m_Physics) m_Physics->Update(deltaTime);
    }

    void Engine::RenderSystems() {
        // Rendering is handled by the graphics frontend
        // This is where you'd update any engine-level rendering
    }

    void Engine::ShutdownSystems() {
        LOG_INFO("Shutting down engine systems...");

        if (m_Graphics) {
            m_Graphics->Shutdown();
            m_Graphics.reset();
            LOG_INFO("Graphics system shutdown");
        }

        m_Physics.reset();
        m_Audio.reset();
        m_Resources.reset();
        m_Input.reset();
        m_Time.reset();

        LOG_INFO("All engine systems shutdown");
    }

    // Global engine access
    Engine* GetEngine() {
        return g_engine_instance;
    }

    void SetEngine(Engine* engine) {
        g_engine_instance = engine;
    }

    // Convenience functions
    bool InitializeEngine(const EngineConfig& config) {
        if (g_engine_instance) {
            LOG_WARN("Engine already exists");
            return true;
        }

        g_engine_instance = new Engine();
        return g_engine_instance->Initialize(config);
    }

    void RunApplication(Application* app) {
        if (!g_engine_instance) {
            LOG_ERROR("Engine not initialized. Call InitializeEngine first.");
            return;
        }

        g_engine_instance->Run(app);
    }

    void ShutdownEngine() {
        if (g_engine_instance) {
            g_engine_instance->Shutdown();
            delete g_engine_instance;
            g_engine_instance = nullptr;
        }
    }

    // Legacy functions for backward compatibility
    int32_t initialize() {
        LOG_INFO("Legacy initialize() called - consider using InitializeEngine()");
        return InitializeEngine() ? 0 : -1;
    }

    int32_t run() {
        LOG_INFO("Legacy run() called - consider using RunApplication()");
        if (g_engine_instance) {
            // Create a dummy application for legacy support
            class LegacyApp : public Application {
            public:
                bool Initialize() override {
                    LOG_INFO("Legacy application running");
                    return true;
                }
                
                void Update(float deltaTime) override {
                    // Basic update loop
                }
                
                void Render() override {
                    // Basic render loop
                }
            };
            
            static LegacyApp legacyApp;
            RunApplication(&legacyApp);
            return 0;
        }
        return -1;
    }

} // namespace lumios