#include "vulkan_backend.h"

#include "core/utils/logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace lumios::graphics::vulkan {
    SwapchainSupportDetails VulkanBackend::QuerySwapchainSupport(VkPhysicalDevice device) {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.present_modes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.present_modes.data());
        }

        return details;
    }

    BackendResult VulkanBackend::CreateSwapchain() {
        // Implementation would create swapchain
        LOG_INFO("Swapchain creation - placeholder implementation");
        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::RecreateSwapchain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_Window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_Window, &width, &height);
            glfwWaitEvents();
        }

        WaitIdle();

        CleanupSwapchain();

        BackendResult result;
        if ((result = CreateSwapchain()) != BackendResult::SUCCESS) return result;
        if ((result = CreateImageViews()) != BackendResult::SUCCESS) return result;

        return BackendResult::SUCCESS;
    }

    void VulkanBackend::CleanupSwapchain() {
        for (auto imageView : m_SwapchainImageViews) {
            vkDestroyImageView(m_Device, imageView, nullptr);
        }

        if (m_Swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        }
    }
}