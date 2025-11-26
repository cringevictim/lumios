#pragma once

#include "defines.h"

#include "../config/graphics_config.h"
#include "../backend/graphics_backend.h"

#include <memory>
#include <functional>
#include <chrono>

struct GLFWwindow;

namespace lumios::graphics {

    // Callback types for external control
    using UpdateCallback = std::function<void(float deltaTime)>;
    using RenderCallback = std::function<void()>;
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    using InputCallback = std::function<void()>;

    enum class FrontendState {
        UNINITIALIZED,
        INITIALIZING,
        RUNNING,
        PAUSED,
        SHUTTING_DOWN,
        ERROR_STATE
    };

    struct FrameStats {
        float delta_time = 0.0f;
        float fps = 0.0f;
        uint64_t frame_count = 0;
        float average_frame_time = 0.0f;
        float min_frame_time = 0.0f;
        float max_frame_time = 0.0f;
    };

    class LUMIOS_API GraphicsFrontend {
    public:
        GraphicsFrontend();
        explicit GraphicsFrontend(const GraphicsConfig& config);
        ~GraphicsFrontend();

        // Core lifecycle
        bool Initialize();
        void Run();
        void Shutdown();

        // State management
        void Pause();
        void Resume();
        void RequestShutdown();
        bool ShouldClose() const;

        // Configuration
        void SetConfig(const GraphicsConfig& config);
        const GraphicsConfig& GetConfig() const { return m_Config; }
        GraphicsConfig& GetConfig() { return m_Config; }

        // Callbacks for external control
        void SetUpdateCallback(UpdateCallback callback) { m_UpdateCallback = callback; }
        void SetRenderCallback(RenderCallback callback) { m_RenderCallback = callback; }
        void SetResizeCallback(ResizeCallback callback) { m_ResizeCallback = callback; }
        void SetInputCallback(InputCallback callback) { m_InputCallback = callback; }

        // State queries
        FrontendState GetState() const { return m_State; }
        bool IsInitialized() const { return m_State != FrontendState::UNINITIALIZED; }
        bool IsRunning() const { return m_State == FrontendState::RUNNING; }
        bool IsPaused() const { return m_State == FrontendState::PAUSED; }

        // Window management
        GLFWwindow* GetWindow() const { return m_Window; }
        void SetWindowTitle(const std::string& title);
        void SetWindowSize(uint32_t width, uint32_t height);
        void SetFullscreen(bool fullscreen);

        // Performance and stats
        const FrameStats& GetFrameStats() const { return m_FrameStats; }
        const RenderStats& GetRenderStats() const;
        void SetTargetFPS(uint32_t fps);
        void SetVSync(bool enabled);

        // Backend access
        GraphicsBackend* GetBackend() const { return m_Backend.get(); }
        std::string GetBackendInfo() const;

    private:
        // Core components
        GraphicsConfig m_Config;
        std::unique_ptr<GraphicsBackend> m_Backend;
        GLFWwindow* m_Window = nullptr;

        // State
        FrontendState m_State = FrontendState::UNINITIALIZED;
        bool m_ShouldClose = false;
        bool m_ResizeRequested = false;
        uint32_t m_NewWidth = 0;
        uint32_t m_NewHeight = 0;

        // Timing
        std::chrono::high_resolution_clock::time_point m_LastFrameTime;
        std::chrono::high_resolution_clock::time_point m_StartTime;
        FrameStats m_FrameStats;
        float m_FrameTimeAccumulator = 0.0f;
        uint32_t m_FrameTimeCount = 0;

        // Callbacks
        UpdateCallback m_UpdateCallback;
        RenderCallback m_RenderCallback;
        ResizeCallback m_ResizeCallback;
        InputCallback m_InputCallback;

        // Initialization helpers
        bool CreateWindow();
        bool InitializeBackend();
        void SetupCallbacks();

        // Main loop components
        void MainLoop();
        void ProcessEvents();
        void UpdateTiming();
        void HandleResize();
        void Update(float deltaTime);
        void Render();

        // Cleanup
        void CleanupBackend();
        void CleanupWindow();

        // Callbacks (static for GLFW)
        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void WindowCloseCallback(GLFWwindow* window);
        static void ErrorCallback(int error, const char* description);

        // Utility
        void SetState(FrontendState state);
        void UpdateFrameStats(float deltaTime);
        std::string StateToString(FrontendState state) const;
    };

} // namespace lumios::graphics