#pragma once

#include "defines.h"
#include "../config/graphics_config.h"
#include <memory>
#include <string>

struct GLFWwindow;

namespace lumios::graphics {

    enum class BackendResult {
        SUCCESS,
        FAILED_INITIALIZATION,
        FAILED_DEVICE_CREATION,
        FAILED_SWAPCHAIN_CREATION,
        FAILED_COMMAND_BUFFER_CREATION,
        FAILED_SYNCHRONIZATION_CREATION,
        DEVICE_LOST,
        OUT_OF_MEMORY,
        SURFACE_LOST,
        UNKNOWN_ERROR
    };

    struct RenderStats {
        uint64_t frames_rendered = 0;
        float frame_time_ms = 0.0f;
        float gpu_time_ms = 0.0f;
        uint32_t draw_calls = 0;
        uint32_t triangles = 0;
        size_t memory_used = 0;
    };

    class LUMIOS_API GraphicsBackend {
    public:
        virtual ~GraphicsBackend() = default;

        // Core lifecycle
        virtual BackendResult Initialize(const GraphicsConfig& config, GLFWwindow* window) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsInitialized() const = 0;

        // Frame operations
        virtual BackendResult BeginFrame() = 0;
        virtual BackendResult EndFrame() = 0;
        virtual BackendResult Present() = 0;

        // Window operations
        virtual BackendResult HandleResize(uint32_t width, uint32_t height) = 0;
        virtual BackendResult RecreateSwapchain() = 0;

        // State management
        virtual void WaitIdle() = 0;
        virtual bool IsDeviceLost() const = 0;

        // Information
        virtual std::string GetAPIName() const = 0;
        virtual std::string GetDeviceName() const = 0;
        virtual std::string GetDriverVersion() const = 0;
        virtual RenderStats GetRenderStats() const = 0;

        // Configuration
        virtual const GraphicsConfig& GetConfig() const = 0;
        virtual bool SupportsFeature(const std::string& feature) const = 0;

    protected:
        GraphicsBackend() = default;
    };

    // Factory function declarations
    LUMIOS_API std::unique_ptr<GraphicsBackend> CreateVulkanBackend();
    LUMIOS_API std::unique_ptr<GraphicsBackend> CreateDirectX12Backend();
    LUMIOS_API std::unique_ptr<GraphicsBackend> CreateBackend(GraphicsAPI api);

} // namespace lumios::graphics
