#include "vulkan_backend.h"

#include "core/utils/logger.h"

namespace lumios::graphics::vulkan {
    BackendResult VulkanBackend::CreateLogicalDevice() {
        LOG_INFO("Creating Logical Device...");

        m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

        if (!m_QueueFamilyIndices.IsComplete()) {
            LOG_ERROR("Selected device doesn't have required queue families");
            return BackendResult::FAILED_INITIALIZATION;
        }

        // Build queue creation info for each unique family
        // Key: family index, Value: number of queues to create
        std::map<uint32_t, uint32_t> queueFamilyMap;

        // Graphics family - request all available queues (up to a reasonable limit)
        uint32_t graphicsFamily = m_QueueFamilyIndices.graphics_family.value();
        uint32_t graphicsQueueCount = (std::min)(m_QueueFamilyIndices.graphics_queue_count, static_cast<uint32_t>(4)); // Cap at 4 for now
        queueFamilyMap[graphicsFamily] = graphicsQueueCount;
        LOG_INFO_F("Requesting {} graphics queues from family {}", graphicsQueueCount, graphicsFamily);

        // Present family (might be same as graphics)
        uint32_t presentFamily = m_QueueFamilyIndices.present_family.value();
        if (presentFamily != graphicsFamily) {
            uint32_t presentQueueCount = (std::min)(m_QueueFamilyIndices.present_queue_count, static_cast<uint32_t>(1));
            queueFamilyMap[presentFamily] = presentQueueCount;
            LOG_INFO_F("Requesting {} present queues from family {}", presentQueueCount, presentFamily);
        }
        else {
            LOG_INFO("Present queue is in the same family as graphics");
        }

        // Async compute family (if different from graphics)
        if (m_QueueFamilyIndices.compute_family.has_value()) {
            uint32_t computeFamily = m_QueueFamilyIndices.compute_family.value();
            if (computeFamily != graphicsFamily) {
                uint32_t computeQueueCount = (std::min)(m_QueueFamilyIndices.compute_queue_count, static_cast<uint32_t>(2));
                queueFamilyMap[computeFamily] = computeQueueCount;
                LOG_INFO_F("Requesting {} async compute queues from family {}", computeQueueCount, computeFamily);
            }
        }

        // Async transfer family (if different from graphics and compute)
        if (m_QueueFamilyIndices.transfer_family.has_value()) {
            uint32_t transferFamily = m_QueueFamilyIndices.transfer_family.value();
            if (transferFamily != graphicsFamily &&
                (!m_QueueFamilyIndices.compute_family.has_value() ||
                    transferFamily != m_QueueFamilyIndices.compute_family.value())) {
                uint32_t transferQueueCount = (std::min)(m_QueueFamilyIndices.transfer_queue_count, static_cast<uint32_t>(1));
                queueFamilyMap[transferFamily] = transferQueueCount;
                LOG_INFO_F("Requesting {} async transfer queues from family {}", transferQueueCount, transferFamily);
            }
        }

        // Create queue create infos with priorities
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<std::vector<float>> queuePriorities; // Keep alive until vkCreateDevice

        for (const auto& [familyIndex, queueCount] : queueFamilyMap) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = familyIndex;
            queueCreateInfo.queueCount = queueCount;

            // Assign priorities (higher priority for first queues)
            std::vector<float> priorities(queueCount);
            for (uint32_t i = 0; i < queueCount; i++) {
                priorities[i] = 1.0f - (static_cast<float>(i) / queueCount) * 0.3f; // Range: 1.0 to 0.7
            }

            queueCreateInfo.pQueuePriorities = priorities.data();
            queuePriorities.push_back(std::move(priorities));
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Specify device features
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;
        // Add more features as needed

        // Create logical device
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Enable device extensions
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Enable validation layers (deprecated but for compatibility)
        if (m_Config.GetRenderConfig().enable_validation) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // Create the logical device
        if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
            LOG_ERROR("Failed to create logical device");
            return BackendResult::FAILED_INITIALIZATION;
        }

        LOG_INFO("Logical device created successfully");

        // Retrieve all queue handles and map them by type
        // Graphics queues
        for (uint32_t i = 0; i < queueFamilyMap[graphicsFamily]; i++) {
            VkQueue queue;
            vkGetDeviceQueue(m_Device, graphicsFamily, i, &queue);
            m_QueuesByFamily[graphicsFamily].push_back(queue);

            if (i == 0) {
                m_Queues[QueueType::GRAPHICS_MAIN] = queue;
                LOG_INFO_F("Graphics main queue retrieved (family={}, index={})", graphicsFamily, i);
            }
            else if (i == 1) {
                m_Queues[QueueType::GRAPHICS_SECONDARY] = queue;
                LOG_INFO_F("Graphics secondary queue retrieved (family={}, index={})", graphicsFamily, i);
            }
        }

        // Present queue
        VkQueue presentQueue;
        vkGetDeviceQueue(m_Device, presentFamily, 0, &presentQueue);
        m_Queues[QueueType::PRESENT] = presentQueue;
        if (presentFamily != graphicsFamily) {
            m_QueuesByFamily[presentFamily].push_back(presentQueue);
        }
        LOG_INFO_F("Present queue retrieved (family={}, index={})", presentFamily, 0);

        // Async compute queue (if exists and different)
        if (m_QueueFamilyIndices.compute_family.has_value()) {
            uint32_t computeFamily = m_QueueFamilyIndices.compute_family.value();
            if (computeFamily != graphicsFamily && queueFamilyMap.count(computeFamily)) {
                VkQueue computeQueue;
                vkGetDeviceQueue(m_Device, computeFamily, 0, &computeQueue);
                m_Queues[QueueType::COMPUTE_ASYNC] = computeQueue;
                m_QueuesByFamily[computeFamily].push_back(computeQueue);
                LOG_INFO_F("Async compute queue retrieved (family={}, index={})", computeFamily, 0);
            }
        }

        // Async transfer queue (if exists and different)
        if (m_QueueFamilyIndices.transfer_family.has_value()) {
            uint32_t transferFamily = m_QueueFamilyIndices.transfer_family.value();
            if (transferFamily != graphicsFamily && queueFamilyMap.count(transferFamily)) {
                VkQueue transferQueue;
                vkGetDeviceQueue(m_Device, transferFamily, 0, &transferQueue);
                m_Queues[QueueType::TRANSFER_ASYNC] = transferQueue;
                m_QueuesByFamily[transferFamily].push_back(transferQueue);
                LOG_INFO_F("Async transfer queue retrieved (family={}, index={})", transferFamily, 0);
            }
        }

        LOG_INFO_F("Total queue types available: {}", m_Queues.size());

        return BackendResult::SUCCESS;
    }
}