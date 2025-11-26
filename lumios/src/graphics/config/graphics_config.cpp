#include "graphics_config.h"
#include "core/utils/logger.h"

#include <string>

// Simple min/max functions
template<typename T>
T simple_min(T a, T b) { return (a < b) ? a : b; }

template<typename T>
T simple_max(T a, T b) { return (a > b) ? a : b; }

namespace lumios::graphics {

    GraphicsConfig::GraphicsConfig() {
        ValidateConfiguration();
    }

    void GraphicsConfig::SetWindowSize(uint32_t width, uint32_t height) {
        m_WindowConfig.width = simple_max(1u, width);
        m_WindowConfig.height = simple_max(1u, height);
        ValidateConfiguration();
    }

    void GraphicsConfig::SetWindowTitle(const std::string& title) {
        m_WindowConfig.title = title.empty() ? std::string("Lumios Engine") : title;
    }

    void GraphicsConfig::SetFullscreen(bool fullscreen) {
        m_WindowConfig.fullscreen = fullscreen;
    }

    void GraphicsConfig::SetResizable(bool resizable) {
        m_WindowConfig.resizable = resizable;
    }

    void GraphicsConfig::SetVSync(bool vsync) {
        m_WindowConfig.vsync = vsync;
        if (vsync) {
            m_RenderConfig.present_mode = PresentMode::FIFO;
        } else {
            m_RenderConfig.present_mode = PresentMode::IMMEDIATE;
        }
    }

    void GraphicsConfig::SetPreferredAPI(GraphicsAPI api) {
        m_RenderConfig.preferred_api = api;
    }

    void GraphicsConfig::SetPresentMode(PresentMode mode) {
        m_RenderConfig.present_mode = mode;
        // Update VSync setting based on present mode
        m_WindowConfig.vsync = (mode == PresentMode::FIFO || mode == PresentMode::FIFO_RELAXED);
    }

    void GraphicsConfig::SetMSAA(MSAASamples samples) {
        m_RenderConfig.msaa_samples = samples;
    }

    void GraphicsConfig::SetValidationEnabled(bool enabled) {
        m_RenderConfig.enable_validation = enabled;
    }

    void GraphicsConfig::SetMaxFramesInFlight(uint32_t frames) {
        m_RenderConfig.max_frames_in_flight = simple_max(1u, simple_min(frames, 8u));
    }

    void GraphicsConfig::SetTargetFPS(uint32_t fps) {
        m_PerformanceConfig.target_fps = simple_max(30u, simple_min(fps, 300u));
    }

    void GraphicsConfig::SetAdaptiveQuality(bool enabled) {
        m_PerformanceConfig.adaptive_quality = enabled;
    }

    bool GraphicsConfig::IsValid() const {
        return m_WindowConfig.width > 0 && 
               m_WindowConfig.height > 0 && 
               !m_WindowConfig.title.empty() &&
               m_RenderConfig.max_frames_in_flight > 0 &&
               m_PerformanceConfig.target_fps > 0;
    }

    std::string GraphicsConfig::GetValidationErrors() const {
        // For simplicity, just return a basic error message
        if (!IsValid()) {
            return std::string("Configuration validation failed");
        }
        return std::string("");
    }

    void GraphicsConfig::ValidateConfiguration() {
        if (!IsValid()) {
            LOG_WARN("Graphics configuration validation failed");
        }
    }

} // namespace lumios::graphics
