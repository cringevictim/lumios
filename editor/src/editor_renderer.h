#pragma once

#include "graphics/vulkan/vk_common.h"
#include "graphics/vulkan/vk_init.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_descriptors.h"
#include "graphics/gpu_types.h"
#include "graphics/camera.h"
#include "imgui.h"

#include <vector>
#include <string>

namespace lumios {

class Window;
class Scene;

class EditorRenderer {
public:
    bool init(Window& window, const std::string& shader_dir);
    void shutdown();

    bool begin_frame();
    void render_scene(Scene& scene, const Camera& camera);
    void begin_ui();
    void end_ui();
    void end_frame();

    void resize_viewport(u32 width, u32 height);
    ImTextureID viewport_texture() const;
    u32 viewport_width()  const { return vp_.width; }
    u32 viewport_height() const { return vp_.height; }

    MeshHandle     upload_mesh(const MeshData& data);
    TextureHandle  load_texture(const std::string& path);
    MaterialHandle create_material(const MaterialData& data);

private:
    VulkanContext   ctx_;
    VulkanSwapchain swapchain_;
    Window*         window_ = nullptr;
    std::string     shader_dir_;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    struct FrameData {
        VkCommandBuffer cmd          = VK_NULL_HANDLE;
        VkSemaphore image_available  = VK_NULL_HANDLE;
        VkSemaphore render_finished  = VK_NULL_HANDLE;
        VkFence     fence            = VK_NULL_HANDLE;
        GPUBuffer   global_ubo, light_ubo;
        VkDescriptorSet global_descriptor = VK_NULL_HANDLE;
    };
    std::vector<FrameData> frames_;
    std::vector<VkFence>   images_in_flight_;
    u32 current_frame_ = 0, image_index_ = 0;

    // UI pass -> swapchain
    VkRenderPass               ui_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> ui_framebuffers_;
    VkDescriptorPool           imgui_pool_ = VK_NULL_HANDLE;

    // Scene pass -> offscreen
    VkRenderPass scene_pass_ = VK_NULL_HANDLE;

    struct ViewportTarget {
        VkImage       color = VK_NULL_HANDLE;
        VmaAllocation color_alloc = VK_NULL_HANDLE;
        VkImageView   color_view = VK_NULL_HANDLE;
        VkSampler     sampler = VK_NULL_HANDLE;
        VkImage       depth = VK_NULL_HANDLE;
        VmaAllocation depth_alloc = VK_NULL_HANDLE;
        VkImageView   depth_view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkDescriptorSet imgui_ds = VK_NULL_HANDLE;
        u32 width = 0, height = 0;
    };
    ViewportTarget vp_;

    // Scene pipeline
    VkPipelineLayout      pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline            pipeline_        = VK_NULL_HANDLE;
    VkDescriptorSetLayout global_layout_   = VK_NULL_HANDLE;
    VkDescriptorSetLayout material_layout_ = VK_NULL_HANDLE;
    DescriptorAllocator   desc_alloc_;

    GPUTexture  default_texture_;
    GPUMaterial default_material_;
    std::vector<GPUMesh>     meshes_;
    std::vector<GPUTexture>  textures_;
    std::vector<GPUMaterial> materials_;

    // Pick pass for entity selection
    struct PickTarget {
        VkImage       color = VK_NULL_HANDLE;
        VmaAllocation color_alloc = VK_NULL_HANDLE;
        VkImageView   color_view = VK_NULL_HANDLE;
        VkImage       depth = VK_NULL_HANDLE;
        VmaAllocation depth_alloc = VK_NULL_HANDLE;
        VkImageView   depth_view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        GPUBuffer     staging;
        u32 width = 0, height = 0;
    };
    PickTarget pick_;
    VkRenderPass       pick_pass_       = VK_NULL_HANDLE;
    VkPipelineLayout   pick_pl_layout_  = VK_NULL_HANDLE;
    VkPipeline         pick_pipeline_   = VK_NULL_HANDLE;

    bool create_ui_pass();
    bool create_ui_framebuffers();
    bool create_scene_pass();
    bool create_viewport_target(u32 w, u32 h);
    void destroy_viewport_target();
    bool create_scene_pipeline();
    bool create_pick_pass();
    bool create_pick_pipeline();
    bool create_pick_target(u32 w, u32 h);
    void destroy_pick_target();
    bool create_frame_resources();
    bool create_default_resources();
    bool init_imgui();
    void cleanup_ui_framebuffers();
    void recreate_swapchain();

public:
    void render_pick(Scene& scene, const Camera& camera);
    u32  read_pick_pixel(u32 x, u32 y);
    VulkanContext& context() { return ctx_; }
    const std::vector<GPUMesh>&     get_meshes()      const { return meshes_; }
    const std::vector<GPUMaterial>& get_materials()    const { return materials_; }
    const GPUMaterial&              get_default_mat()  const { return default_material_; }
    const std::string&              get_shader_dir()   const { return shader_dir_; }
};

} // namespace lumios
