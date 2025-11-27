#include "vulkan_backend.h"
#include "core/utils/logger.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <set>
#include <algorithm>
#include <cstring>


namespace lumios::graphics::vulkan {

    // Validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

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

        if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
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

        VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

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

    bool VulkanBackend::SupportsFeature(const std::string& feature) const {
        // Basic feature support checking - can be expanded
        if (feature == "validation_layers") {
            return m_Config.GetRenderConfig().enable_validation;
        }
        if (feature == "debug_markers") {
            return m_Config.GetRenderConfig().enable_debug_markers;
        }
        return false;
    }

    BackendResult VulkanBackend::CreateInstance() {
        LOG_INFO("Requesting validation layers...");
        if (m_Config.GetRenderConfig().enable_validation && !CheckValidationLayerSupport()) {
            LOG_ERROR("Validation layers requested, but not available");
            return BackendResult::FAILED_INITIALIZATION;
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = m_Config.GetWindowConfig().title.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Lumios Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = GetRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (m_Config.GetRenderConfig().enable_validation) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = DebugCallback;

            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
            LOG_ERROR("Failed to create Vulkan instance");
            return BackendResult::FAILED_INITIALIZATION;
        }


        return BackendResult::SUCCESS;
    }

    bool VulkanBackend::CheckValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> VulkanBackend::GetRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (m_Config.GetRenderConfig().enable_validation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void VulkanBackend::CleanupSwapchain() {
        for (auto imageView : m_SwapchainImageViews) {
            vkDestroyImageView(m_Device, imageView, nullptr);
        }

        if (m_Swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanBackend::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG_ERROR_F("[VALIDATION] {}", pCallbackData->pMessage);
        } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG_WARN_F("[VALIDATION] {}", pCallbackData->pMessage);
        } else {
            LOG_INFO_F("[VALIDATION] {}", pCallbackData->pMessage);
		}

        return VK_FALSE;
    }

    BackendResult VulkanBackend::SetupDebugMessenger() {
        if (!m_Config.GetRenderConfig().enable_validation) return BackendResult::SUCCESS;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            if (func(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
                LOG_ERROR("Failed to set up debug messenger");
                return BackendResult::FAILED_INITIALIZATION;
            }
        } else {
            LOG_ERROR("Debug messenger extension not available");
            return BackendResult::FAILED_INITIALIZATION;
        }

        return BackendResult::SUCCESS;
    }

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

    BackendResult VulkanBackend::PickPhysicalDevice() {
        LOG_INFO("Selecting physical device...");

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            LOG_ERROR("Failed to find GPUs with Vulkan support, shutting down...");
            return BackendResult::FAILED_INITIALIZATION;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        LOG_INFO_F("Found {} GPU(s) with Vulkan support", deviceCount);

        uint32_t bestScore = 0;
        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

        for (uint32_t i = 0; i < deviceCount; i++) {
            VkPhysicalDevice device = devices[i];

            VkPhysicalDeviceProperties deviceProperties;
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            uint32_t score = 0;

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 10000;
            }
            else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                score += 1000;
            }
            else {
                score += 100;
            }

            score += deviceProperties.limits.maxImageDimension2D;

            QueueFamilyIndices indices = FindQueueFamilies(device);
            if (!indices.IsComplete()) {
                score = 0;
            }

            if (score > 0 && !CheckDeviceExtensionSupport(device)) {
                score = 0;
            }

            if (score > 0) {
                SwapchainSupportDetails swapchainSupport = QuerySwapchainSupport(device);
                if (swapchainSupport.formats.empty() || swapchainSupport.present_modes.empty()) {
                    score = 0;
                }
            }

            if (deviceFeatures.geometryShader) score += 100;
            if (deviceFeatures.samplerAnisotropy) score += 50;

            const char* deviceType = "Unknown";
            switch (deviceProperties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   deviceType = "Discrete"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "Integrated"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    deviceType = "Virtual"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            deviceType = "CPU"; break;
            }

            LOG_INFO_F("GPU [{}]: {} ({}) - Score: {}", i, deviceProperties.deviceName, deviceType, score);

            if (score > bestScore) {
                bestScore = score;
                bestDevice = device;
            }
        }

        if (bestDevice == VK_NULL_HANDLE || bestScore == 0) {
            LOG_ERROR("Failed to find a suitable GPU");
            return BackendResult::FAILED_INITIALIZATION;
        }

        m_PhysicalDevice = bestDevice;

        VkPhysicalDeviceProperties selectedProps;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &selectedProps);
        LOG_INFO_F("Selected GPU: {} (Score: {})", selectedProps.deviceName, bestScore);

        return BackendResult::SUCCESS;
    }

    bool VulkanBackend::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices VulkanBackend::FindQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }

            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices.compute_family = i;
            }

            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                indices.transfer_family = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            if (presentSupport) {
                indices.present_family = i;
            }

            if (indices.IsComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

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

    BackendResult VulkanBackend::CreateLogicalDevice() {
        LOG_INFO("Creating Logical Device...");


        return BackendResult::SUCCESS;
    }

    BackendResult VulkanBackend::CreateSwapchain() {
        // Implementation would create swapchain
        LOG_INFO("Swapchain creation - placeholder implementation");
        return BackendResult::SUCCESS;
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
