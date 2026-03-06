#pragma once

#include "graphics/vulkan/vk_common.h"
#include "graphics/vulkan/vk_init.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_descriptors.h"
#include "graphics/gpu_types.h"
#include "graphics/camera.h"
#include "scene/scene.h"
#include "scene/components.h"
#include "scripting/script_manager.h"

struct GLFWwindow;

namespace lumios::editor {

class GameWindow {
public:
    bool open(VulkanContext& shared_ctx, const std::string& shader_dir,
              const std::vector<GPUMesh>& meshes, const std::vector<GPUMaterial>& materials,
              const GPUMaterial& default_mat);
    void close();
    bool is_open() const { return window_ != nullptr; }
    bool should_close() const;

    void render_frame(Scene& scene, ScriptManager* scripts, float dt);

private:
    GLFWwindow*     window_ = nullptr;
    VulkanContext*  ctx_     = nullptr;
    VkSurfaceKHR    surface_ = VK_NULL_HANDLE;
    VulkanSwapchain swapchain_;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    struct FrameData {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkSemaphore render_finished = VK_NULL_HANDLE;
        VkFence     fence           = VK_NULL_HANDLE;
        GPUBuffer   global_ubo, light_ubo;
        VkDescriptorSet global_descriptor = VK_NULL_HANDLE;
    };
    std::vector<FrameData> frames_;
    std::vector<VkFence>   images_in_flight_;
    u32 current_frame_ = 0, image_index_ = 0;
    bool need_swapchain_recreate_ = false;

    VkRenderPass          render_pass_     = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkPipelineLayout      pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline            pipeline_        = VK_NULL_HANDLE;
    VkDescriptorSetLayout global_layout_   = VK_NULL_HANDLE;
    VkDescriptorSetLayout material_layout_ = VK_NULL_HANDLE;
    DescriptorAllocator   desc_alloc_;

    const std::vector<GPUMesh>*     meshes_ptr_     = nullptr;
    const std::vector<GPUMaterial>* materials_ptr_  = nullptr;
    const GPUMaterial*              default_mat_ptr_ = nullptr;

    bool create_render_pass();
    bool create_framebuffers();
    void cleanup_framebuffers();
    bool create_pipeline(const std::string& shader_dir);
    bool create_frame_resources();
    void recreate_swapchain();

    Camera resolve_game_camera(Scene& scene);
};

} // namespace lumios::editor
