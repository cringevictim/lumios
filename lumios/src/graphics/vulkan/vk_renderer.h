#pragma once

#include "../renderer.h"
#include "vk_common.h"
#include "vk_init.h"
#include "vk_swapchain.h"
#include "vk_descriptors.h"
#include <array>

namespace lumios {

class VulkanRenderer : public Renderer {
    VulkanContext    ctx_;
    VulkanSwapchain  swapchain_;
    VkRenderPass     render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    struct FrameData {
        VkCommandPool   command_pool   = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkSemaphore     image_available = VK_NULL_HANDLE;
        VkSemaphore     render_finished = VK_NULL_HANDLE;
        VkFence         in_flight       = VK_NULL_HANDLE;
        GPUBuffer       global_ubo;
        GPUBuffer       light_ubo;
        VkDescriptorSet global_descriptor = VK_NULL_HANDLE;
    };

    std::vector<FrameData> frames_;
    u32 frame_count_   = 0;
    u32 current_frame_ = 0;
    u32 image_index_   = 0;

    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline       pipeline_        = VK_NULL_HANDLE;

    DescriptorAllocator descriptor_alloc_;
    VkDescriptorSetLayout global_set_layout_   = VK_NULL_HANDLE;
    VkDescriptorSetLayout material_set_layout_ = VK_NULL_HANDLE;

    GPUTexture  default_texture_;
    GPUMaterial default_material_;

    std::vector<GPUMesh>     meshes_;
    std::vector<GPUTexture>  textures_;
    std::vector<GPUMaterial> materials_;
    std::vector<VkFence>     images_in_flight_;

    Window* window_  = nullptr;
    std::string shader_dir_;

    bool create_render_pass();
    bool create_framebuffers();
    bool create_pipeline();
    bool create_frame_resources();
    bool create_descriptors();
    bool create_default_resources();
    void cleanup_swapchain_dependent();
    void recreate_swapchain();

public:
    bool init(Window& window, const std::string& shader_dir) override;
    void shutdown() override;
    bool begin_frame() override;
    void end_frame() override;

    MeshHandle     upload_mesh(const MeshData& data) override;
    TextureHandle  load_texture(const std::string& path) override;
    MaterialHandle create_material(const MaterialData& data) override;
    void           render_scene(Scene& scene, const Camera& camera) override;
};

} // namespace lumios
