#include "vulkan_backend.h"

#include "core/utils/logger.h"

#include <set>
#include <string>

namespace lumios::graphics::vulkan {

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

            m_QueueFamilyIndices = FindQueueFamilies(device);
            if (!m_QueueFamilyIndices.IsComplete()) {
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

        LOG_INFO_F("Found {} queue families", queueFamilyCount);

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            const auto& queueFamily = queueFamilies[i];

            LOG_INFO_F("Queue Family {}: queueCount={}, flags={}",
                i, queueFamily.queueCount, queueFamily.queueFlags);

            // Graphics family
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (!indices.graphics_family.has_value()) {
                    indices.graphics_family = i;
                    indices.graphics_queue_count = queueFamily.queueCount;
                    //LOG_INFO_F("  -> Graphics queue found at index {} ({} queues available)", i, queueFamily.queueCount);
                }
            }

            // Compute family (prefer dedicated if available)
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                if (!indices.compute_family.has_value() ||
                    !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    indices.compute_family = i;
                    indices.compute_queue_count = queueFamily.queueCount;
                    //LOG_INFO_F("  -> Compute queue found at index {} ({} queues available)", i, queueFamily.queueCount);
                }
            }

            // Transfer family (prefer dedicated if available)
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (!indices.transfer_family.has_value() ||
                    !(queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) {
                    indices.transfer_family = i;
                    indices.transfer_queue_count = queueFamily.queueCount;
                    //LOG_INFO_F("  -> Transfer queue found at index {} ({} queues available)", i, queueFamily.queueCount);
                }
            }

            // Present family
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            if (presentSupport) {
                if (!indices.present_family.has_value()) {
                    indices.present_family = i;
                    indices.present_queue_count = queueFamily.queueCount;
                    //LOG_INFO_F("  -> Present queue found at index {} ({} queues available)", i, queueFamily.queueCount);
                }
            }

            if (indices.IsComplete()) {
                LOG_INFO("All required queue families found!");
            }
        }

        // Final status
        if (!indices.IsComplete()) {
            LOG_WARN("Not all required queue families were found!");
            if (!indices.graphics_family.has_value()) {
                LOG_ERROR("Missing graphics queue family");
            }
            if (!indices.present_family.has_value()) {
                LOG_ERROR("Missing present queue family");
            }
        }

        return indices;
    }
}