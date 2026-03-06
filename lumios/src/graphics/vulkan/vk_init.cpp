#include "vk_init.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <set>
#include <string>

namespace lumios {

static const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        LOG_ERROR("[Vulkan] %s", data->pMessage);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        LOG_WARN("[Vulkan] %s", data->pMessage);
    return VK_FALSE;
}

static bool check_validation_support() {
    u32 count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (auto* name : VALIDATION_LAYERS) {
        bool found = false;
        for (auto& l : layers) if (strcmp(name, l.layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

static VkResult create_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT* out) {
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debug_callback;

    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    return fn ? fn(instance, &ci, nullptr, out) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn) fn(instance, messenger, nullptr);
}

static int score_device(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);

    // Check required extensions
    u32 ext_count;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, exts.data());

    std::set<std::string> required(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (auto& e : exts) required.erase(e.extensionName);
    if (!required.empty()) return -1;

    // Check swapchain support
    u32 fmt_count, pm_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmt_count, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pm_count, nullptr);
    if (fmt_count == 0 || pm_count == 0) return -1;

    // Check queue families
    u32 qf_count;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qf.data());

    bool has_graphics = false, has_present = false;
    for (u32 i = 0; i < qf_count; i++) {
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) has_graphics = true;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) has_present = true;
    }
    if (!has_graphics || !has_present) return -1;

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;
    score += props.limits.maxImageDimension2D / 1000;
    return score;
}

bool VulkanContext::init(GLFWwindow* window) {
    // --- Instance ---
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Lumios Application";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Lumios";
    app_info.engineVersion = VK_MAKE_VERSION(LUMIOS_VERSION_MAJOR, LUMIOS_VERSION_MINOR, LUMIOS_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_3;

    u32 glfw_ext_count;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);

    bool enable_validation = LUMIOS_DEBUG && check_validation_support();
    if (enable_validation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<u32>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    if (enable_validation) {
        ci.enabledLayerCount = static_cast<u32>(VALIDATION_LAYERS.size());
        ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    VK_CHECK(vkCreateInstance(&ci, nullptr, &instance));
    LOG_INFO("Vulkan instance created (API 1.3)");

    if (enable_validation) {
        create_debug_messenger(instance, &debug_messenger);
        LOG_DEBUG("Validation layers enabled");
    }

    // --- Surface ---
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    // --- Physical device ---
    u32 dev_count;
    vkEnumeratePhysicalDevices(instance, &dev_count, nullptr);
    if (dev_count == 0) { LOG_FATAL("No Vulkan-capable GPU found"); return false; }

    std::vector<VkPhysicalDevice> devices(dev_count);
    vkEnumeratePhysicalDevices(instance, &dev_count, devices.data());

    int best_score = -1;
    for (auto& d : devices) {
        int s = score_device(d, surface);
        if (s > best_score) { best_score = s; physical_device = d; }
    }
    if (best_score < 0) { LOG_FATAL("No suitable GPU found"); return false; }

    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    LOG_INFO("GPU: %s", device_properties.deviceName);

    // --- Queue families ---
    u32 qf_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qf_count, qf.data());

    bool found_graphics = false, found_present = false;
    for (u32 i = 0; i < qf_count; i++) {
        if (!found_graphics && (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphics_family = i;
            found_graphics = true;
        }
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present);
        if (!found_present && present) {
            present_family = i;
            found_present = true;
        }
    }

    // --- Logical device ---
    std::set<u32> unique_families = {graphics_family, present_family};
    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    float priority = 1.0f;
    for (u32 fam : unique_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        queue_cis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = static_cast<u32>(queue_cis.size());
    dci.pQueueCreateInfos = queue_cis.data();
    dci.enabledExtensionCount = static_cast<u32>(DEVICE_EXTENSIONS.size());
    dci.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    dci.pEnabledFeatures = &features;

    VK_CHECK(vkCreateDevice(physical_device, &dci, nullptr, &device));

    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_family, 0, &present_queue);
    LOG_INFO("Logical device created");

    // --- VMA ---
    VmaAllocatorCreateInfo alloc_ci{};
    alloc_ci.physicalDevice = physical_device;
    alloc_ci.device = device;
    alloc_ci.instance = instance;
    alloc_ci.vulkanApiVersion = VK_API_VERSION_1_3;

    VK_CHECK(vmaCreateAllocator(&alloc_ci, &allocator));
    LOG_INFO("VMA allocator created");

    return true;
}

void VulkanContext::shutdown() {
    if (allocator)       { vmaDestroyAllocator(allocator); allocator = VK_NULL_HANDLE; }
    if (device)          { vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE; }
    if (debug_messenger) { destroy_debug_messenger(instance, debug_messenger); debug_messenger = VK_NULL_HANDLE; }
    if (surface)         { vkDestroySurfaceKHR(instance, surface, nullptr); surface = VK_NULL_HANDLE; }
    if (instance)        { vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
    LOG_INFO("Vulkan context destroyed");
}

VkCommandBuffer VulkanContext::begin_single_command(VkCommandPool pool) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = pool;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &ai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    return cmd;
}

void VulkanContext::end_single_command(VkCommandPool pool, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkQueueSubmit(graphics_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

} // namespace lumios
