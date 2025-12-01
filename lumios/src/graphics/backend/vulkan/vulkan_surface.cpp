#include "vulkan_backend.h"

#include "core/utils/logger.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan_win32.h>

namespace lumios::graphics::vulkan {
    BackendResult VulkanBackend::CreateSurface() {
        LOG_INFO("Creating Surface...");
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.hinstance = GetModuleHandle(nullptr);
        createInfo.hwnd = glfwGetWin32Window(m_Window);


        if (vkCreateWin32SurfaceKHR(m_Instance, &createInfo, nullptr, &m_Surface) != VK_SUCCESS) {
            LOG_ERROR("Failed to create window surface");
            return BackendResult::FAILED_INITIALIZATION;
        }

        /*VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);*/
        /*LOG_INFO(capabilities.currentExtent.height);*/

        return BackendResult::SUCCESS;
    }
}