#include "vulkan_backend.h"
#include "core/utils/logger.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

namespace lumios::graphics::vulkan {

    VulkanBackend::VulkanBackend() = default;

    VulkanBackend::~VulkanBackend() {
        Shutdown();
    }

    BackendResult VulkanBackend::Initialize(const GraphicsConfig& config, GLFWwindow* window) {
        if (m_IsInitialized) {
            LOG_WARN("VulkanBackend already initialized");
            return BackendResult::SUCCESS;
        }

        m_Config = config;
        m_Window = window;

        LOG_INFO("Initializing Vulkan backend...");

        BackendResult result;

        if ((result = CreateInstance()) != BackendResult::SUCCESS) return result;
        if ((result = SetupDebugMessenger()) != BackendResult::SUCCESS) return result;
        if ((result = CreateSurface()) != BackendResult::SUCCESS) return result;
        if ((result = PickPhysicalDevice()) != BackendResult::SUCCESS) return result;
        if ((result = CreateLogicalDevice()) != BackendResult::SUCCESS) return result;
        if ((result = CreateSwapchain()) != BackendResult::SUCCESS) return result;
        if ((result = CreateImageViews()) != BackendResult::SUCCESS) return result;
        if ((result = CreateCommandPool()) != BackendResult::SUCCESS) return result;
        if ((result = CreateCommandBuffers()) != BackendResult::SUCCESS) return result;
        if ((result = CreateSyncObjects()) != BackendResult::SUCCESS) return result;

        m_IsInitialized = true;
        LOG_INFO("Vulkan backend initialized successfully");
        return BackendResult::SUCCESS;
    }

    void VulkanBackend::Shutdown() {
        if (!m_IsInitialized) return;

        LOG_INFO("Shutting down Vulkan backend...");

        WaitIdle();

        // Cleanup synchronization objects
        for (size_t i = 0; i < m_Config.GetRenderConfig().max_frames_in_flight; i++) {
            if (m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
            }
            if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
            }
            if (m_InFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
            }
        }

        if (m_CommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        }

        CleanupSwapchain();

        if (m_Device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_Device, nullptr);
        }

        if (m_Config.GetRenderConfig().enable_validation && m_DebugMessenger != VK_NULL_HANDLE) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(m_Instance, m_DebugMessenger, nullptr);
            }
        }

        if (m_Surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        }

        if (m_Instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_Instance, nullptr);
        }

        m_IsInitialized = false;
        LOG_INFO("Vulkan backend shutdown complete");
    }

    BackendResult VulkanBackend::BeginFrame() {
        if (!m_IsInitialized) return BackendResult::FAILED_INITIALIZATION;

        // Wait for the previous frame to finish
        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        // Acquire next image from swapchain
        VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, 
            m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_ImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
            return BackendResult::SUCCESS;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            LOG_ERROR("Failed to acquire swapchain image");
            return BackendResult::FAILED_SWAPCHAIN_CREATION;
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

        // Reset command buffer
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
            LOG_ERROR("Failed to begin recording command buffer");
            return BackendResult::FAILED_COMMAND_BUFFER_CREATION;
        }

        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::EndFrame() {
        if (!m_IsInitialized) return BackendResult::FAILED_INITIALIZATION;

        // End command buffer recording
        if (vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS) {
            LOG_ERROR("Failed to record command buffer");
            return BackendResult::FAILED_COMMAND_BUFFER_CREATION;
        }

        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::Present() {
        if (!m_IsInitialized) return BackendResult::FAILED_INITIALIZATION;

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkQueue graphicsQueue = GetQueue(QueueType::GRAPHICS_MAIN);
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
            LOG_ERROR("Failed to submit draw command buffer");
            return BackendResult::UNKNOWN_ERROR;
        }

        // Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = { m_Swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &m_ImageIndex;

        VkQueue presentQueue = GetQueue(QueueType::PRESENT);
        VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
            m_FramebufferResized = false;
            RecreateSwapchain();
        } else if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to present swapchain image");
            return BackendResult::UNKNOWN_ERROR;
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % m_Config.GetRenderConfig().max_frames_in_flight;
        m_Stats.frames_rendered++;

        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::HandleResize(uint32_t width, uint32_t height) {
        m_FramebufferResized = true;
        return BackendResult::SUCCESS;
    }

    void VulkanBackend::WaitIdle() {
        if (m_Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_Device);
        }
    }

    std::string VulkanBackend::GetDeviceName() const {
        if (m_PhysicalDevice == VK_NULL_HANDLE) return "Unknown";
        
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
        return std::string(properties.deviceName);
    }

    std::string VulkanBackend::GetDriverVersion() const {
        if (m_PhysicalDevice == VK_NULL_HANDLE) return "Unknown";
        
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
        
        uint32_t major = VK_VERSION_MAJOR(properties.driverVersion);
        uint32_t minor = VK_VERSION_MINOR(properties.driverVersion);
        uint32_t patch = VK_VERSION_PATCH(properties.driverVersion);
        
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    RenderStats VulkanBackend::GetRenderStats() const {
        return m_Stats;
    }
    
    VkQueue VulkanBackend::GetQueue(QueueType type) const {
        auto it = m_Queues.find(type);
        if (it != m_Queues.end()) {
            return it->second;
        }
        return VK_NULL_HANDLE;
    }

    bool VulkanBackend::HasQueue(QueueType type) const {
        return m_Queues.find(type) != m_Queues.end();
    }

    BackendResult VulkanBackend::CreateImageViews() {
        // Implementation would create image views
        LOG_INFO("Image views creation - placeholder implementation");
        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::CreateCommandPool() {
        // Implementation would create command pool
        LOG_INFO("Command pool creation - placeholder implementation");
        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::CreateCommandBuffers() {
        // Implementation would create command buffers
        LOG_INFO("Command buffers creation - placeholder implementation");
        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::CreateSyncObjects() {
        // Implementation would create synchronization objects
        LOG_INFO("Sync objects creation - placeholder implementation");
        return BackendResult::SUCCESS;
    }

} // namespace lumios::graphics
