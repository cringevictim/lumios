#include "graphics_frontend.h"

#include "core/utils/logger.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <thread>

namespace lumios::graphics {

    GraphicsFrontend::GraphicsFrontend() {
        LOG_INFO("GraphicsFrontend created with default configuration");
    }

    GraphicsFrontend::GraphicsFrontend(const GraphicsConfig& config) : m_Config(config) {
        LOG_INFO("GraphicsFrontend created with custom configuration");
    }

    GraphicsFrontend::~GraphicsFrontend() {
        Shutdown();
    }

    bool GraphicsFrontend::Initialize() {
        if (m_State != FrontendState::UNINITIALIZED) {
            LOG_WARN("GraphicsFrontend already initialized or in invalid state: {}", StateToString(m_State));
            return m_State == FrontendState::RUNNING || m_State == FrontendState::PAUSED;
        }

        SetState(FrontendState::INITIALIZING);
        LOG_INFO("Initializing GraphicsFrontend...");

        // Create window
        if (!CreateWindow()) {
            SetState(FrontendState::ERROR_STATE);
            return false;
        }

        // Initialize graphics backend
        if (!InitializeBackend()) {
            SetState(FrontendState::ERROR_STATE);
            return false;
        }

        // Setup callbacks
        SetupCallbacks();

        // Initialize timing
        m_StartTime = std::chrono::high_resolution_clock::now();
        m_LastFrameTime = m_StartTime;

        SetState(FrontendState::RUNNING);
        LOG_INFO("GraphicsFrontend initialized successfully");
        return true;
    }

    void GraphicsFrontend::Run() {
        if (m_State != FrontendState::RUNNING) {
            LOG_ERROR("Cannot run GraphicsFrontend - not in running state: {}", StateToString(m_State));
            return;
        }

        LOG_INFO("Starting main loop...");
        MainLoop();
        LOG_INFO("Main loop ended");
    }

    void GraphicsFrontend::Shutdown() {
        if (m_State == FrontendState::UNINITIALIZED) {
            return;
        }

        SetState(FrontendState::SHUTTING_DOWN);
        LOG_INFO("Shutting down GraphicsFrontend...");

        CleanupBackend();
        CleanupWindow();

        SetState(FrontendState::UNINITIALIZED);
        LOG_INFO("GraphicsFrontend shutdown complete");
    }

    void GraphicsFrontend::Pause() {
        if (m_State == FrontendState::RUNNING) {
            SetState(FrontendState::PAUSED);
            LOG_INFO("GraphicsFrontend paused");
        }
    }

    void GraphicsFrontend::Resume() {
        if (m_State == FrontendState::PAUSED) {
            SetState(FrontendState::RUNNING);
            m_LastFrameTime = std::chrono::high_resolution_clock::now();
            LOG_INFO("GraphicsFrontend resumed");
        }
    }

    void GraphicsFrontend::RequestShutdown() {
        m_ShouldClose = true;
        LOG_INFO("Shutdown requested");
    }

    bool GraphicsFrontend::ShouldClose() const {
        return m_ShouldClose || (m_Window && glfwWindowShouldClose(m_Window));
    }

    void GraphicsFrontend::SetConfig(const GraphicsConfig& config) {
        m_Config = config;
        LOG_INFO("Graphics configuration updated");
    }

    void GraphicsFrontend::SetWindowTitle(const std::string& title) {
        m_Config.SetWindowTitle(title);
        if (m_Window) {
            glfwSetWindowTitle(m_Window, title.c_str());
        }
    }

    void GraphicsFrontend::SetWindowSize(uint32_t width, uint32_t height) {
        m_Config.SetWindowSize(width, height);
        if (m_Window) {
            glfwSetWindowSize(m_Window, static_cast<int>(width), static_cast<int>(height));
        }
    }

    void GraphicsFrontend::SetFullscreen(bool fullscreen) {
        m_Config.SetFullscreen(fullscreen);
        // Fullscreen implementation would go here
        LOG_INFO("Fullscreen mode: {}", fullscreen ? "enabled" : "disabled");
    }

    const RenderStats& GraphicsFrontend::GetRenderStats() const {
        static RenderStats empty_stats;
        if (m_Backend) {
            return m_Backend->GetRenderStats();
        }
        return empty_stats;
    }

    void GraphicsFrontend::SetTargetFPS(uint32_t fps) {
        m_Config.SetTargetFPS(fps);
        LOG_INFO("Target FPS set to: {}", fps);
    }

    void GraphicsFrontend::SetVSync(bool enabled) {
        m_Config.SetVSync(enabled);
        LOG_INFO("VSync: {}", enabled ? "enabled" : "disabled");
    }

    std::string GraphicsFrontend::GetBackendInfo() const {
        if (!m_Backend) return "No backend";
        
        return m_Backend->GetAPIName() + " - " + 
               m_Backend->GetDeviceName() + " (Driver: " + 
               m_Backend->GetDriverVersion() + ")";
    }

    // Private implementation methods
    bool GraphicsFrontend::CreateWindow() {
        const auto& windowConfig = m_Config.GetWindowConfig();

        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize GLFW");
            return false;
        }

        if (m_Config.GetRenderConfig().preferred_api == GraphicsAPI::VULKAN) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        glfwWindowHint(GLFW_RESIZABLE, windowConfig.resizable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, windowConfig.decorated ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, windowConfig.maximized ? GLFW_TRUE : GLFW_FALSE);

        m_Window = glfwCreateWindow(
            static_cast<int32_t>(windowConfig.width),
            static_cast<int32_t>(windowConfig.height),
            windowConfig.title.c_str(),
            windowConfig.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
            nullptr
        );

        if (!m_Window) {
            LOG_ERROR("Failed to create GLFW window");
            return false;
        }

        glfwSetWindowUserPointer(m_Window, this);
        LOG_INFO_F("Window created: {}x{} '{}'", windowConfig.width, windowConfig.height, windowConfig.title);
        return true;
    }

    bool GraphicsFrontend::InitializeBackend() {
        m_Backend = CreateBackend(m_Config.GetRenderConfig().preferred_api);

        if (!m_Backend) {
            LOG_ERROR("Failed to create graphics backend. ");
            return false;
        }

        BackendResult result = m_Backend->Initialize(m_Config, m_Window);
        if (result != BackendResult::SUCCESS) {
            LOG_ERROR("Failed to initialize graphics backend");
            m_Backend.reset();
            return false;
        }

        LOG_INFO("Graphics backend initialized: {}", m_Backend->GetAPIName());
        return true;
    }

    void GraphicsFrontend::SetupCallbacks() {
        glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
        glfwSetWindowCloseCallback(m_Window, WindowCloseCallback);
    }

    void GraphicsFrontend::MainLoop() {
        while (!ShouldClose() && m_State == FrontendState::RUNNING) {
            ProcessEvents();
            UpdateTiming();
            HandleResize();

            if (m_State == FrontendState::RUNNING) {
                Update(m_FrameStats.delta_time);
                Render();
            }

            // Frame rate limiting
            const auto& perfConfig = m_Config.GetPerformanceConfig();
            if (perfConfig.target_fps > 0 && !m_Config.GetWindowConfig().vsync) {
                auto target_frame_time = std::chrono::microseconds(1000000 / perfConfig.target_fps);
                auto frame_end = std::chrono::high_resolution_clock::now();
                auto frame_duration = frame_end - m_LastFrameTime;
                
                if (frame_duration < target_frame_time) {
                    std::this_thread::sleep_for(target_frame_time - frame_duration);
                }
            }
        }
    }

    void GraphicsFrontend::ProcessEvents() {
        glfwPollEvents();
        
        if (m_InputCallback) {
            m_InputCallback();
        }
    }

    void GraphicsFrontend::UpdateTiming() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration<float>(current_time - m_LastFrameTime).count();
        
        m_LastFrameTime = current_time;
        UpdateFrameStats(delta);
    }

    void GraphicsFrontend::HandleResize() {
        if (m_ResizeRequested) {
            if (m_Backend) {
                m_Backend->HandleResize(m_NewWidth, m_NewHeight);
            }
            
            if (m_ResizeCallback) {
                m_ResizeCallback(m_NewWidth, m_NewHeight);
            }
            
            m_ResizeRequested = false;
        }
    }

    void GraphicsFrontend::Update(float deltaTime) {
        if (m_UpdateCallback) {
            m_UpdateCallback(deltaTime);
        }
    }

    void GraphicsFrontend::Render() {
        if (!m_Backend) return;

        BackendResult result = m_Backend->BeginFrame();
        if (result != BackendResult::SUCCESS) {
            LOG_ERROR("Failed to begin frame");
            return;
        }

        if (m_RenderCallback) {
            m_RenderCallback();
        }

        result = m_Backend->EndFrame();
        if (result != BackendResult::SUCCESS) {
            LOG_ERROR("Failed to end frame");
            return;
        }

        result = m_Backend->Present();
        if (result != BackendResult::SUCCESS) {
            LOG_ERROR("Failed to present frame");
        }
    }

    void GraphicsFrontend::CleanupBackend() {
        if (m_Backend) {
            m_Backend->Shutdown();
            m_Backend.reset();
        }
    }

    void GraphicsFrontend::CleanupWindow() {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            glfwTerminate();
        }
    }


    // Static callbacks

    void GraphicsFrontend::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        auto* frontend = static_cast<GraphicsFrontend*>(glfwGetWindowUserPointer(window));
        if (frontend) {
            frontend->m_ResizeRequested = true;
            frontend->m_NewWidth = static_cast<uint32_t>(width);
            frontend->m_NewHeight = static_cast<uint32_t>(height);
        }
    }

    void GraphicsFrontend::WindowCloseCallback(GLFWwindow* window) {
        auto* frontend = static_cast<GraphicsFrontend*>(glfwGetWindowUserPointer(window));
        if (frontend) {
            frontend->RequestShutdown();
        }
    }

    void GraphicsFrontend::ErrorCallback(int error, const char* description) {
        LOG_ERROR("GLFW Error {}: {}", error, description);
    }

    // Utility methods

    void GraphicsFrontend::SetState(FrontendState state) {
        if (m_State != state) {
            LOG_DEBUG("State change: {} -> {}", StateToString(m_State), StateToString(state));
            m_State = state;
        }
    }

    void GraphicsFrontend::UpdateFrameStats(float deltaTime) {
        m_FrameStats.delta_time = deltaTime;
        m_FrameStats.frame_count++;
        
        if (deltaTime > 0.0f) {
            m_FrameStats.fps = 1.0f / deltaTime;
        }

        // Update min/max frame times
        if (m_FrameStats.frame_count == 1) {
            m_FrameStats.min_frame_time = deltaTime;
            m_FrameStats.max_frame_time = deltaTime;
        } else {
            m_FrameStats.min_frame_time = std::min(m_FrameStats.min_frame_time, deltaTime);
            m_FrameStats.max_frame_time = std::max(m_FrameStats.max_frame_time, deltaTime);
        }

        // Calculate rolling average
        m_FrameTimeAccumulator += deltaTime;
        m_FrameTimeCount++;
        
        if (m_FrameTimeCount >= 60) { // Update average every 60 frames
            m_FrameStats.average_frame_time = m_FrameTimeAccumulator / m_FrameTimeCount;
            m_FrameTimeAccumulator = 0.0f;
            m_FrameTimeCount = 0;
        }
    }

    std::string GraphicsFrontend::StateToString(FrontendState state) const {
        switch (state) {
            case FrontendState::UNINITIALIZED: return "UNINITIALIZED";
            case FrontendState::INITIALIZING: return "INITIALIZING";
            case FrontendState::RUNNING: return "RUNNING";
            case FrontendState::PAUSED: return "PAUSED";
            case FrontendState::SHUTTING_DOWN: return "SHUTTING_DOWN";
            case FrontendState::ERROR_STATE: return "ERROR_STATE";
            default: return "UNKNOWN";
        }
    }

} // namespace lumios::graphics
