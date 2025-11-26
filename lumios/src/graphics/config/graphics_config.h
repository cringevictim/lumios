#pragma once

#include "defines.h"

#include <string>

// Basic type definitions
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef int int32_t;
typedef long long int64_t;
typedef float float32_t;

namespace lumios::graphics {

    enum class GraphicsAPI {
        VULKAN,
        DIRECTX12,
        AUTO_SELECT
    };

    enum class PresentMode {
        IMMEDIATE,      // No VSync
        FIFO,          // VSync
        FIFO_RELAXED,  // Adaptive VSync
        MAILBOX        // Triple buffering
    };

    enum class MSAASamples {
        NONE = 1,
        X2 = 2,
        X4 = 4,
        X8 = 8,
        X16 = 16
    };

    struct WindowConfig {
        uint32_t width = 1280;
        uint32_t height = 720;
        std::string title = "Lumios Engine";
        bool fullscreen = false;
        bool resizable = true;
        bool decorated = true;
        bool maximized = false;
        bool vsync = true;
    };

    struct RenderConfig {
        GraphicsAPI preferred_api = GraphicsAPI::AUTO_SELECT;
        PresentMode present_mode = PresentMode::FIFO;
        MSAASamples msaa_samples = MSAASamples::NONE;
        bool enable_validation = true;
        bool enable_debug_markers = true;
        uint32_t max_frames_in_flight = 2;
        bool enable_anisotropic_filtering = true;
        float max_anisotropy = 16.0f;
    };

    struct PerformanceConfig {
        bool enable_gpu_timing = false;
        bool enable_cpu_timing = false;
        uint32_t target_fps = 60;
        bool adaptive_quality = false;
    };

    class LUMIOS_API GraphicsConfig {
    public:
        GraphicsConfig();
        ~GraphicsConfig() = default;

        // Window configuration
        void SetWindowSize(uint32_t width, uint32_t height);
        void SetWindowTitle(const std::string& title);
        void SetFullscreen(bool fullscreen);
        void SetResizable(bool resizable);
        void SetVSync(bool vsync);

        // Render configuration
        void SetPreferredAPI(GraphicsAPI api);
        void SetPresentMode(PresentMode mode);
        void SetMSAA(MSAASamples samples);
        void SetValidationEnabled(bool enabled);
        void SetMaxFramesInFlight(uint32_t frames);

        // Performance configuration
        void SetTargetFPS(uint32_t fps);
        void SetAdaptiveQuality(bool enabled);

        // Getters
        const WindowConfig& GetWindowConfig() const { return m_WindowConfig; }
        const RenderConfig& GetRenderConfig() const { return m_RenderConfig; }
        const PerformanceConfig& GetPerformanceConfig() const { return m_PerformanceConfig; }

        WindowConfig& GetWindowConfig() { return m_WindowConfig; }
        RenderConfig& GetRenderConfig() { return m_RenderConfig; }
        PerformanceConfig& GetPerformanceConfig() { return m_PerformanceConfig; }

        // Validation
        bool IsValid() const;
        std::string GetValidationErrors() const;

    private:
        WindowConfig m_WindowConfig;
        RenderConfig m_RenderConfig;
        PerformanceConfig m_PerformanceConfig;

        void ValidateConfiguration();
    };

} // namespace lumios::graphics
