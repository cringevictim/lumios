#include "graphics_backend.h"
#include "vulkan/vulkan_backend.h"
#include "directx12/dx12_backend.h"
#include "core/utils/logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace lumios::graphics {

    std::unique_ptr<GraphicsBackend> CreateVulkanBackend() {
        return std::make_unique<vulkan::VulkanBackend>();
    }

    std::unique_ptr<GraphicsBackend> CreateDirectX12Backend() {
        return std::make_unique<DirectX12Backend>();
    }

    std::unique_ptr<GraphicsBackend> CreateBackend(GraphicsAPI api) {
        switch (api) {
            case GraphicsAPI::VULKAN:
                LOG_INFO("Creating Vulkan backend");
                return CreateVulkanBackend();
                
            case GraphicsAPI::DIRECTX12:
                LOG_INFO("Creating DirectX 12 backend");
                return CreateDirectX12Backend();
                
            case GraphicsAPI::AUTO_SELECT:
                LOG_INFO("Auto-selecting graphics backend");
                
                // Auto-selection logic
#ifdef _WIN32
                // On Windows, prefer DirectX 12 if available, fallback to Vulkan
                // For now, since DX12 is stub, prefer Vulkan
                LOG_INFO("Windows detected - selecting Vulkan (DirectX 12 is stub)");
                return CreateVulkanBackend();
#else
                // On other platforms, use Vulkan
                LOG_INFO("Non-Windows platform detected - selecting Vulkan");
                return CreateVulkanBackend();
#endif
           

            default:
                //LOG_ERROR("Unknown graphics API requested");
                return nullptr;
        }
    }

} // namespace lumios::graphics
