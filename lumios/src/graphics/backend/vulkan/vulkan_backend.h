#pragma once

#include "../graphics_backend.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <array>
#include <map>

struct GLFWwindow;

namespace lumios::graphics::vulkan {

    // Queue type enumeration for easy access
    enum class QueueType : uint8_t {
        GRAPHICS_MAIN = 0,      // Primary graphics queue
        GRAPHICS_SECONDARY,     // Secondary graphics queue (for parallel submission)
        PRESENT,                // Presentation queue (often same as graphics)
        COMPUTE_ASYNC,          // Async compute queue
        TRANSFER_ASYNC,         // Async transfer/DMA queue
        COUNT                   // Total number of queue types
    };

    // Queue family information with available queue counts
    struct QueueFamilyInfo {
        uint32_t family_index = UINT32_MAX;
        uint32_t available_queues = 0;
        VkQueueFlags flags = 0;
        bool supports_present = false;

        bool IsValid() const { return family_index != UINT32_MAX; }
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;
        std::optional<uint32_t> compute_family;
        std::optional<uint32_t> transfer_family;

        // Store available queue counts for each family
        uint32_t graphics_queue_count = 0;
        uint32_t present_queue_count = 0;
        uint32_t compute_queue_count = 0;
        uint32_t transfer_queue_count = 0;

        bool IsComplete() const {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    class LUMIOS_API VulkanBackend : public GraphicsBackend {
    public:
        VulkanBackend();
        ~VulkanBackend() override;

        // GraphicsBackend interface
        BackendResult Initialize(const GraphicsConfig& config, GLFWwindow* window) override;
        void Shutdown() override;
        bool IsInitialized() const override { return m_IsInitialized; }

        BackendResult BeginFrame() override;
        BackendResult EndFrame() override;
        BackendResult Present() override;

        BackendResult HandleResize(uint32_t width, uint32_t height) override;
        BackendResult RecreateSwapchain() override;

        void WaitIdle() override;
        bool IsDeviceLost() const override { return m_DeviceLost; }

        std::string GetAPIName() const override { return "Vulkan"; }
        std::string GetDeviceName() const override;
        std::string GetDriverVersion() const override;
        RenderStats GetRenderStats() const override;

        const GraphicsConfig& GetConfig() const override { return m_Config; }

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;

        // Queue management
        std::map<QueueType, VkQueue> m_Queues;  // Map of queue types to queue handles
        std::map<uint32_t, std::vector<VkQueue>> m_QueuesByFamily;  // All queues organized by family
        QueueFamilyIndices m_QueueFamilyIndices;

        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainImageViews;
        VkFormat m_SwapchainImageFormat;
        VkExtent2D m_SwapchainExtent;

        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_CommandBuffers;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;

        GraphicsConfig m_Config;
        GLFWwindow* m_Window = nullptr;
        bool m_IsInitialized = false;
        bool m_DeviceLost = false;
        bool m_FramebufferResized = false;
        uint32_t m_CurrentFrame = 0;
        uint32_t m_ImageIndex = 0;

        mutable RenderStats m_Stats;

        // Initialization helpers
        BackendResult CreateInstance();
        BackendResult SetupDebugMessenger();
        BackendResult CreateSurface();
        BackendResult PickPhysicalDevice();
        BackendResult CreateLogicalDevice();
        BackendResult CreateSwapchain();
        BackendResult CreateImageViews();
        BackendResult CreateCommandPool();
        BackendResult CreateCommandBuffers();
        BackendResult CreateSyncObjects();

        // Helper functions
        bool CheckValidationLayerSupport();
        std::vector<const char*> GetRequiredExtensions();
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
        SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device);
        //VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
        //VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
        //VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

        // Queue access helpers
        VkQueue GetQueue(QueueType type) const;
        bool HasQueue(QueueType type) const;

        // Cleanup helpers
        void CleanupSwapchain();

        // Debug callback
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type,
            const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
            void* user_data);
    };

} // namespace lumios::graphics
