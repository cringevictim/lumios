#include "editor_renderer.h"
#include "graphics/vulkan/vk_pipeline.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vk_texture.h"
#include "platform/window.h"
#include "scene/scene.h"
#include "scene/components.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ImGuizmo.h"

namespace lumios {

// ─── Init / Shutdown ─────────────────────────────────────────────────

bool EditorRenderer::init(Window& window, const std::string& shader_dir) {
    window_ = &window;
    shader_dir_ = shader_dir;

    if (!ctx_.init(window.handle())) return false;

    int w, h;
    window.get_framebuffer_size(w, h);
    if (!swapchain_.create(ctx_, w, h)) return false;

    VkCommandPoolCreateInfo pci{};
    pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = ctx_.graphics_family;
    VK_CHECK(vkCreateCommandPool(ctx_.device, &pci, nullptr, &command_pool_));

    if (!create_ui_pass()) return false;
    if (!create_ui_framebuffers()) return false;
    if (!create_scene_pass()) return false;

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 200},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200}
    };
    auto span = std::span<VkDescriptorPoolSize>(pool_sizes, 2);
    desc_alloc_.init(ctx_.device, 300, span);

    global_layout_ = DescriptorLayoutBuilder()
        .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .add(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx_.device);
    material_layout_ = DescriptorLayoutBuilder()
        .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .add(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx_.device);

    if (!create_scene_pipeline()) return false;
    if (!create_pick_pass()) return false;
    if (!create_pick_pipeline()) return false;
    if (!create_pick_target(800, 600)) return false;
    if (!create_frame_resources()) return false;
    if (!create_default_resources()) return false;
    if (!init_imgui()) return false;
    if (!create_viewport_target(800, 600)) return false;

    LOG_INFO("Editor renderer initialized");
    return true;
}

void EditorRenderer::shutdown() {
    vkDeviceWaitIdle(ctx_.device);

    destroy_viewport_target();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    destroy_texture(ctx_, default_texture_);
    destroy_buffer(ctx_.allocator, default_material_.ubo);
    for (auto& m : materials_) destroy_buffer(ctx_.allocator, m.ubo);
    for (auto& t : textures_)  destroy_texture(ctx_, t);
    for (auto& m : meshes_) {
        destroy_buffer(ctx_.allocator, m.vertex_buffer);
        destroy_buffer(ctx_.allocator, m.index_buffer);
    }

    for (auto& f : frames_) {
        destroy_buffer(ctx_.allocator, f.global_ubo);
        destroy_buffer(ctx_.allocator, f.light_ubo);
        vkDestroyFence(ctx_.device, f.fence, nullptr);
        vkDestroySemaphore(ctx_.device, f.render_finished, nullptr);
        vkDestroySemaphore(ctx_.device, f.image_available, nullptr);
    }

    destroy_pick_target();
    if (pick_pipeline_)   vkDestroyPipeline(ctx_.device, pick_pipeline_, nullptr);
    if (pick_pl_layout_)  vkDestroyPipelineLayout(ctx_.device, pick_pl_layout_, nullptr);
    if (pick_pass_)       vkDestroyRenderPass(ctx_.device, pick_pass_, nullptr);
    desc_alloc_.destroy(ctx_.device);
    if (pipeline_)        vkDestroyPipeline(ctx_.device, pipeline_, nullptr);
    if (pipeline_layout_) vkDestroyPipelineLayout(ctx_.device, pipeline_layout_, nullptr);
    if (material_layout_) vkDestroyDescriptorSetLayout(ctx_.device, material_layout_, nullptr);
    if (global_layout_)   vkDestroyDescriptorSetLayout(ctx_.device, global_layout_, nullptr);
    if (scene_pass_)      vkDestroyRenderPass(ctx_.device, scene_pass_, nullptr);
    // ImGui manages its own descriptor pool via DescriptorPoolSize
    cleanup_ui_framebuffers();
    if (ui_pass_)         vkDestroyRenderPass(ctx_.device, ui_pass_, nullptr);
    vkDestroyCommandPool(ctx_.device, command_pool_, nullptr);
    swapchain_.cleanup(ctx_);
    ctx_.shutdown();
}

// ─── UI render pass (to swapchain) ──────────────────────────────────

bool EditorRenderer::create_ui_pass() {
    VkAttachmentDescription att{};
    att.format         = swapchain_.image_format;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &att;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;
    VK_CHECK(vkCreateRenderPass(ctx_.device, &ci, nullptr, &ui_pass_));
    return ui_pass_ != VK_NULL_HANDLE;
}

bool EditorRenderer::create_ui_framebuffers() {
    ui_framebuffers_.resize(swapchain_.image_views.size());
    for (size_t i = 0; i < swapchain_.image_views.size(); i++) {
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = ui_pass_;
        ci.attachmentCount = 1;
        ci.pAttachments    = &swapchain_.image_views[i];
        ci.width           = swapchain_.extent.width;
        ci.height          = swapchain_.extent.height;
        ci.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(ctx_.device, &ci, nullptr, &ui_framebuffers_[i]));
    }
    return true;
}

void EditorRenderer::cleanup_ui_framebuffers() {
    for (auto fb : ui_framebuffers_) vkDestroyFramebuffer(ctx_.device, fb, nullptr);
    ui_framebuffers_.clear();
}

// ─── Scene render pass (offscreen) ──────────────────────────────────

bool EditorRenderer::create_scene_pass() {
    VkAttachmentDescription atts[2]{};
    // Color
    atts[0].format         = VK_FORMAT_R8G8B8A8_UNORM;
    atts[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    atts[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    atts[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[0].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth
    atts[1].format         = VK_FORMAT_D32_SFLOAT;
    atts[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    atts[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 1;
    sub.pColorAttachments       = &color_ref;
    sub.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 2;
    ci.pAttachments    = atts;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    ci.dependencyCount = 2;
    ci.pDependencies   = deps;
    VK_CHECK(vkCreateRenderPass(ctx_.device, &ci, nullptr, &scene_pass_));
    return scene_pass_ != VK_NULL_HANDLE;
}

// ─── Offscreen viewport target ──────────────────────────────────────

bool EditorRenderer::create_viewport_target(u32 w, u32 h) {
    vp_.width  = w > 0 ? w : 1;
    vp_.height = h > 0 ? h : 1;

    // Color image
    VkImageCreateInfo ici{};
    ici.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType   = VK_IMAGE_TYPE_2D;
    ici.format      = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent      = {vp_.width, vp_.height, 1};
    ici.mipLevels   = 1;
    ici.arrayLayers = 1;
    ici.samples     = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling      = VK_IMAGE_TILING_OPTIMAL;
    ici.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateImage(ctx_.allocator, &ici, &aci, &vp_.color, &vp_.color_alloc, nullptr));

    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = vp_.color;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(ctx_.device, &vi, nullptr, &vp_.color_view));

    VkSamplerCreateInfo si{};
    si.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(ctx_.device, &si, nullptr, &vp_.sampler));

    // Depth image
    ici.format = VK_FORMAT_D32_SFLOAT;
    ici.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VK_CHECK(vmaCreateImage(ctx_.allocator, &ici, &aci, &vp_.depth, &vp_.depth_alloc, nullptr));

    vi.image  = vp_.depth;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VK_CHECK(vkCreateImageView(ctx_.device, &vi, nullptr, &vp_.depth_view));

    // Framebuffer
    VkImageView views[] = {vp_.color_view, vp_.depth_view};
    VkFramebufferCreateInfo fci{};
    fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass      = scene_pass_;
    fci.attachmentCount = 2;
    fci.pAttachments    = views;
    fci.width           = vp_.width;
    fci.height          = vp_.height;
    fci.layers          = 1;
    VK_CHECK(vkCreateFramebuffer(ctx_.device, &fci, nullptr, &vp_.framebuffer));

    // ImGui descriptor for displaying the viewport texture
    vp_.imgui_ds = ImGui_ImplVulkan_AddTexture(vp_.sampler, vp_.color_view,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return true;
}

void EditorRenderer::destroy_viewport_target() {
    if (vp_.imgui_ds)   ImGui_ImplVulkan_RemoveTexture(vp_.imgui_ds);
    if (vp_.framebuffer) vkDestroyFramebuffer(ctx_.device, vp_.framebuffer, nullptr);
    if (vp_.depth_view)  vkDestroyImageView(ctx_.device, vp_.depth_view, nullptr);
    if (vp_.depth)       vmaDestroyImage(ctx_.allocator, vp_.depth, vp_.depth_alloc);
    if (vp_.sampler)     vkDestroySampler(ctx_.device, vp_.sampler, nullptr);
    if (vp_.color_view)  vkDestroyImageView(ctx_.device, vp_.color_view, nullptr);
    if (vp_.color)       vmaDestroyImage(ctx_.allocator, vp_.color, vp_.color_alloc);
    vp_ = {};
}

void EditorRenderer::resize_viewport(u32 w, u32 h) {
    if (w == vp_.width && h == vp_.height) return;
    vkDeviceWaitIdle(ctx_.device);
    destroy_viewport_target();
    create_viewport_target(w, h);
}

ImTextureID EditorRenderer::viewport_texture() const {
    return reinterpret_cast<ImTextureID>(vp_.imgui_ds);
}

// ─── Scene pipeline ─────────────────────────────────────────────────

bool EditorRenderer::create_scene_pipeline() {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.size       = sizeof(PushConstants);

    VkDescriptorSetLayout layouts[] = {global_layout_, material_layout_};
    VkPipelineLayoutCreateInfo li{};
    li.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    li.setLayoutCount         = 2;
    li.pSetLayouts            = layouts;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges    = &push;
    VK_CHECK(vkCreatePipelineLayout(ctx_.device, &li, nullptr, &pipeline_layout_));

    auto vert = load_shader_module(ctx_.device, shader_dir_ + "/mesh.vert.spv");
    auto frag = load_shader_module(ctx_.device, shader_dir_ + "/mesh.frag.spv");
    if (!vert || !frag) { LOG_ERROR("Editor: Failed to load shaders"); return false; }

    pipeline_ = PipelineBuilder()
        .set_shaders(vert, frag)
        .set_vertex_layout()
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .enable_depth_test(true, VK_COMPARE_OP_LESS)
        .disable_blending()
        .set_layout(pipeline_layout_)
        .build(ctx_.device, scene_pass_);

    vkDestroyShaderModule(ctx_.device, vert, nullptr);
    vkDestroyShaderModule(ctx_.device, frag, nullptr);
    return pipeline_ != VK_NULL_HANDLE;
}

// ─── Pick pass (entity selection) ───────────────────────────────────

struct PickPushConstants {
    glm::mat4 model;
    u32 entity_id;
};

bool EditorRenderer::create_pick_pass() {
    VkAttachmentDescription atts[2]{};
    atts[0].format         = VK_FORMAT_R8G8B8A8_UNORM;
    atts[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    atts[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    atts[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[0].finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    atts[1].format         = VK_FORMAT_D32_SFLOAT;
    atts[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    atts[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 1;
    sub.pColorAttachments       = &color_ref;
    sub.pDepthStencilAttachment = &depth_ref;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 2;
    ci.pAttachments    = atts;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    VK_CHECK(vkCreateRenderPass(ctx_.device, &ci, nullptr, &pick_pass_));
    return pick_pass_ != VK_NULL_HANDLE;
}

bool EditorRenderer::create_pick_pipeline() {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.size       = sizeof(PickPushConstants);

    VkDescriptorSetLayout layouts[] = {global_layout_};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount         = 1;
    li.pSetLayouts            = layouts;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges    = &push;
    VK_CHECK(vkCreatePipelineLayout(ctx_.device, &li, nullptr, &pick_pl_layout_));

    auto vert = load_shader_module(ctx_.device, shader_dir_ + "/pick.vert.spv");
    auto frag = load_shader_module(ctx_.device, shader_dir_ + "/pick.frag.spv");
    if (!vert || !frag) { LOG_WARN("Pick shaders not found - entity selection disabled"); return true; }

    pick_pipeline_ = PipelineBuilder()
        .set_shaders(vert, frag)
        .set_vertex_layout()
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .enable_depth_test(true, VK_COMPARE_OP_LESS)
        .disable_blending()
        .set_layout(pick_pl_layout_)
        .build(ctx_.device, pick_pass_);

    vkDestroyShaderModule(ctx_.device, vert, nullptr);
    vkDestroyShaderModule(ctx_.device, frag, nullptr);
    return true;
}

bool EditorRenderer::create_pick_target(u32 w, u32 h) {
    pick_.width  = w > 0 ? w : 1;
    pick_.height = h > 0 ? h : 1;

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType   = VK_IMAGE_TYPE_2D;
    ici.format      = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent      = {pick_.width, pick_.height, 1};
    ici.mipLevels   = 1;
    ici.arrayLayers = 1;
    ici.samples     = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling      = VK_IMAGE_TILING_OPTIMAL;
    ici.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VK_CHECK(vmaCreateImage(ctx_.allocator, &ici, &aci, &pick_.color, &pick_.color_alloc, nullptr));

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image    = pick_.color;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(ctx_.device, &vi, nullptr, &pick_.color_view));

    ici.format = VK_FORMAT_D32_SFLOAT;
    ici.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VK_CHECK(vmaCreateImage(ctx_.allocator, &ici, &aci, &pick_.depth, &pick_.depth_alloc, nullptr));

    vi.image  = pick_.depth;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VK_CHECK(vkCreateImageView(ctx_.device, &vi, nullptr, &pick_.depth_view));

    VkImageView views[] = {pick_.color_view, pick_.depth_view};
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass      = pick_pass_;
    fci.attachmentCount = 2;
    fci.pAttachments    = views;
    fci.width           = pick_.width;
    fci.height          = pick_.height;
    fci.layers          = 1;
    VK_CHECK(vkCreateFramebuffer(ctx_.device, &fci, nullptr, &pick_.framebuffer));

    pick_.staging = create_buffer(ctx_.allocator, 4,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
    return true;
}

void EditorRenderer::destroy_pick_target() {
    if (pick_.staging.buffer) destroy_buffer(ctx_.allocator, pick_.staging);
    if (pick_.framebuffer) vkDestroyFramebuffer(ctx_.device, pick_.framebuffer, nullptr);
    if (pick_.depth_view)  vkDestroyImageView(ctx_.device, pick_.depth_view, nullptr);
    if (pick_.depth)       vmaDestroyImage(ctx_.allocator, pick_.depth, pick_.depth_alloc);
    if (pick_.color_view)  vkDestroyImageView(ctx_.device, pick_.color_view, nullptr);
    if (pick_.color)       vmaDestroyImage(ctx_.allocator, pick_.color, pick_.color_alloc);
    pick_ = {};
}

void EditorRenderer::render_pick(Scene& scene, const Camera& camera) {
    if (!pick_pipeline_ || pick_.width == 0) return;

    if (vp_.width != pick_.width || vp_.height != pick_.height) {
        vkDeviceWaitIdle(ctx_.device);
        destroy_pick_target();
        create_pick_target(vp_.width, vp_.height);
    }

    auto& f = frames_[current_frame_];

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass  = pick_pass_;
    rpbi.framebuffer = pick_.framebuffer;
    rpbi.renderArea  = {{0, 0}, {pick_.width, pick_.height}};
    VkClearValue clears[2];
    clears[0].color        = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clears[1].depthStencil = {1.0f, 0};
    rpbi.clearValueCount = 2;
    rpbi.pClearValues    = clears;

    vkCmdBeginRenderPass(f.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x      = 0;
    vp.y      = static_cast<float>(pick_.height);
    vp.width  = static_cast<float>(pick_.width);
    vp.height = -static_cast<float>(pick_.height);
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd, 0, 1, &vp);

    VkRect2D scissor{{0, 0}, {pick_.width, pick_.height}};
    vkCmdSetScissor(f.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pick_pipeline_);
    vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pick_pl_layout_,
                            0, 1, &f.global_descriptor, 0, nullptr);

    auto mv = scene.view<Transform, MeshComponent>();
    for (auto entity : mv) {
        auto& tr = mv.get<Transform>(entity);
        auto& mc = mv.get<MeshComponent>(entity);
        if (!mc.mesh.valid() || mc.mesh.index >= meshes_.size()) continue;

        PickPushConstants pc{};
        pc.model     = tr.matrix();
        pc.entity_id = static_cast<u32>(entity);
        vkCmdPushConstants(f.cmd, pick_pl_layout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(pc), &pc);

        auto& gm = meshes_[mc.mesh.index];
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(f.cmd, 0, 1, &gm.vertex_buffer.buffer, &off);
        vkCmdBindIndexBuffer(f.cmd, gm.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(f.cmd, gm.index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(f.cmd);
}

u32 EditorRenderer::read_pick_pixel(u32 x, u32 y) {
    if (!pick_.staging.buffer || x >= pick_.width || y >= pick_.height) return UINT32_MAX;

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = command_pool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(ctx_.device, &ai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset      = {static_cast<i32>(x), static_cast<i32>(y), 0};
    region.imageExtent      = {1, 1, 1};
    vkCmdCopyImageToBuffer(cmd, pick_.color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           pick_.staging.buffer, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;
    vkQueueSubmit(ctx_.graphics_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_.graphics_queue);
    vkFreeCommandBuffers(ctx_.device, command_pool_, 1, &cmd);

    u8* data;
    vmaMapMemory(ctx_.allocator, pick_.staging.allocation, reinterpret_cast<void**>(&data));
    u32 id = data[0] | (data[1] << 8) | (data[2] << 16);
    vmaUnmapMemory(ctx_.allocator, pick_.staging.allocation);

    return id;
}

// ─── Frame resources ────────────────────────────────────────────────

bool EditorRenderer::create_frame_resources() {
    u32 count = static_cast<u32>(swapchain_.images.size());
    frames_.resize(count);
    images_in_flight_.resize(count, VK_NULL_HANDLE);

    for (auto& f : frames_) {
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = command_pool_;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(ctx_.device, &ai, &f.cmd));

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(ctx_.device, &sci, nullptr, &f.image_available));
        VK_CHECK(vkCreateSemaphore(ctx_.device, &sci, nullptr, &f.render_finished));

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(ctx_.device, &fci, nullptr, &f.fence));

        f.global_ubo = create_buffer(ctx_.allocator, sizeof(GlobalUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        f.light_ubo = create_buffer(ctx_.allocator, sizeof(LightUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        f.global_descriptor = desc_alloc_.allocate(ctx_.device, global_layout_);
        DescriptorWriter()
            .write_buffer(0, f.global_ubo.buffer, sizeof(GlobalUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .write_buffer(1, f.light_ubo.buffer, sizeof(LightUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .update(ctx_.device, f.global_descriptor);
    }
    return true;
}

// ─── Default resources ──────────────────────────────────────────────

bool EditorRenderer::create_default_resources() {
    default_texture_ = create_default_white_texture(ctx_, command_pool_);

    MaterialUBOData md{};
    md.base_color = {1,1,1,1};
    md.roughness  = 0.5f;
    md.ao         = 1.0f;

    default_material_.ubo = create_buffer(ctx_.allocator, sizeof(MaterialUBOData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    upload_buffer_data(ctx_.allocator, default_material_.ubo, &md, sizeof(md));

    default_material_.descriptor = desc_alloc_.allocate(ctx_.device, material_layout_);
    DescriptorWriter()
        .write_buffer(0, default_material_.ubo.buffer, sizeof(MaterialUBOData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .write_image(1, default_texture_.view, default_texture_.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .update(ctx_.device, default_material_.descriptor);
    return true;
}

// ─── ImGui ──────────────────────────────────────────────────────────

bool EditorRenderer::init_imgui() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.0f;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 3.0f;
    style.FrameRounding     = 2.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(5, 3);
    style.ItemSpacing       = ImVec2(6, 4);

    auto& c = style.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.07f, 0.07f, 0.09f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.18f, 0.18f, 0.22f, 0.60f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.16f, 0.16f, 0.20f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.20f, 0.25f, 1.0f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.04f, 0.05f, 1.0f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.04f, 0.04f, 0.05f, 1.0f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.05f, 0.6f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.28f, 0.34f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.36f, 0.36f, 0.42f, 1.0f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.40f, 0.65f, 0.90f, 1.0f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.30f, 0.50f, 0.78f, 1.0f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.38f, 0.58f, 0.88f, 1.0f);
    c[ImGuiCol_Button]               = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.22f, 0.28f, 1.0f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.28f, 0.28f, 0.36f, 1.0f);
    c[ImGuiCol_Header]               = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.18f, 0.24f, 1.0f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.22f, 0.22f, 0.30f, 1.0f);
    c[ImGuiCol_Separator]            = ImVec4(0.16f, 0.16f, 0.20f, 1.0f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.30f, 0.50f, 0.78f, 0.78f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.30f, 0.50f, 0.78f, 1.0f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.50f, 0.78f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.30f, 0.50f, 0.78f, 0.67f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.30f, 0.50f, 0.78f, 0.95f);
    c[ImGuiCol_Tab]                  = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.18f, 0.18f, 0.24f, 1.0f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_TabDimmed]            = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
    c[ImGuiCol_DockingPreview]       = ImVec4(0.30f, 0.50f, 0.78f, 0.70f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.04f, 0.04f, 0.05f, 1.0f);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.30f, 0.50f, 0.78f, 0.35f);

    ImGuizmo::Style& gz = ImGuizmo::GetStyle();
    gz.TranslationLineThickness = 4.0f;
    gz.TranslationLineArrowSize = 8.0f;
    gz.RotationLineThickness    = 3.5f;
    gz.RotationOuterLineThickness = 4.0f;
    gz.ScaleLineThickness       = 4.0f;
    gz.ScaleLineCircleSize      = 8.0f;
    gz.CenterCircleSize         = 7.0f;
    gz.Colors[ImGuizmo::DIRECTION_X]  = ImVec4(0.95f, 0.20f, 0.20f, 1.0f);
    gz.Colors[ImGuizmo::DIRECTION_Y]  = ImVec4(0.20f, 0.90f, 0.20f, 1.0f);
    gz.Colors[ImGuizmo::DIRECTION_Z]  = ImVec4(0.20f, 0.40f, 0.95f, 1.0f);
    gz.Colors[ImGuizmo::PLANE_X]      = ImVec4(0.95f, 0.20f, 0.20f, 0.30f);
    gz.Colors[ImGuizmo::PLANE_Y]      = ImVec4(0.20f, 0.90f, 0.20f, 0.30f);
    gz.Colors[ImGuizmo::PLANE_Z]      = ImVec4(0.20f, 0.40f, 0.95f, 0.30f);
    gz.Colors[ImGuizmo::SELECTION]    = ImVec4(1.0f,  0.95f, 0.40f, 1.0f);
    gz.Colors[ImGuizmo::INACTIVE]     = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);

    ImGui_ImplGlfw_InitForVulkan(window_->handle(), true);

    ImGui_ImplVulkan_InitInfo init{};
    init.ApiVersion      = VK_API_VERSION_1_3;
    init.Instance        = ctx_.instance;
    init.PhysicalDevice  = ctx_.physical_device;
    init.Device          = ctx_.device;
    init.QueueFamily     = ctx_.graphics_family;
    init.Queue           = ctx_.graphics_queue;
    init.DescriptorPoolSize = 100;
    init.MinImageCount   = static_cast<u32>(swapchain_.images.size());
    init.ImageCount      = static_cast<u32>(swapchain_.images.size());
    init.PipelineInfoMain.RenderPass   = ui_pass_;
    init.PipelineInfoMain.MSAASamples  = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init);

    LOG_INFO("ImGui initialized");
    return true;
}

// ─── Swapchain recreation ───────────────────────────────────────────

void EditorRenderer::recreate_swapchain() {
    int w = 0, h = 0;
    window_->get_framebuffer_size(w, h);
    while (w == 0 || h == 0) { window_->get_framebuffer_size(w, h); glfwWaitEvents(); }

    vkDeviceWaitIdle(ctx_.device);
    cleanup_ui_framebuffers();

    auto old = swapchain_.handle;
    swapchain_.handle = VK_NULL_HANDLE;
    if (swapchain_.depth_view)  vkDestroyImageView(ctx_.device, swapchain_.depth_view, nullptr);
    if (swapchain_.depth_image) vmaDestroyImage(ctx_.allocator, swapchain_.depth_image, swapchain_.depth_allocation);
    for (auto v : swapchain_.image_views) vkDestroyImageView(ctx_.device, v, nullptr);
    swapchain_.image_views.clear();
    swapchain_.images.clear();

    swapchain_.create(ctx_, w, h, old);
    if (old) vkDestroySwapchainKHR(ctx_.device, old, nullptr);

    images_in_flight_.assign(swapchain_.images.size(), VK_NULL_HANDLE);
    create_ui_framebuffers();
}

// ─── Frame lifecycle ────────────────────────────────────────────────

bool EditorRenderer::begin_frame() {
    auto& f = frames_[current_frame_];
    vkWaitForFences(ctx_.device, 1, &f.fence, VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(ctx_.device, swapchain_.handle, UINT64_MAX,
                                          f.image_available, VK_NULL_HANDLE, &image_index_);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) { recreate_swapchain(); return false; }

    if (images_in_flight_[image_index_] != VK_NULL_HANDLE)
        vkWaitForFences(ctx_.device, 1, &images_in_flight_[image_index_], VK_TRUE, UINT64_MAX);
    images_in_flight_[image_index_] = f.fence;

    vkResetFences(ctx_.device, 1, &f.fence);
    vkResetCommandBuffer(f.cmd, 0);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(f.cmd, &bi));
    return true;
}

void EditorRenderer::render_scene(Scene& scene, const Camera& camera) {
    auto& f = frames_[current_frame_];

    GlobalUBO global{};
    global.view          = camera.view();
    global.projection    = camera.projection();
    global.camera_pos    = glm::vec4(camera.position(), 1.0f);
    global.ambient_color = glm::vec4(0.08f, 0.08f, 0.12f, 0.3f);

    LightUBO light_data{};
    int lc = 0;
    auto lv = scene.view<Transform, LightComponent>();
    for (auto e : lv) {
        if (lc >= 16) break;
        auto& t = lv.get<Transform>(e);
        auto& l = lv.get<LightComponent>(e);
        auto& gl = light_data.lights[lc];
        gl.position  = glm::vec4(t.position, l.type == LightType::Directional ? 0.0f : 1.0f);
        gl.color     = glm::vec4(l.color, l.intensity);
        gl.direction = glm::vec4(glm::normalize(glm::vec3(
            cos(glm::radians(t.rotation.y)) * cos(glm::radians(t.rotation.x)),
            sin(glm::radians(t.rotation.x)),
            sin(glm::radians(t.rotation.y)) * cos(glm::radians(t.rotation.x))
        )), 0.0f);
        gl.params = glm::vec4(l.range, cos(glm::radians(l.spot_angle)),
                              static_cast<float>(static_cast<int>(l.type)), 0.0f);
        lc++;
    }
    global.num_lights = lc;

    upload_buffer_data(ctx_.allocator, f.global_ubo, &global, sizeof(global));
    upload_buffer_data(ctx_.allocator, f.light_ubo, &light_data, sizeof(light_data));

    // Begin offscreen render pass
    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass  = scene_pass_;
    rpbi.framebuffer = vp_.framebuffer;
    rpbi.renderArea  = {{0, 0}, {vp_.width, vp_.height}};
    VkClearValue clears[2];
    clears[0].color        = {{0.05f, 0.05f, 0.07f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};
    rpbi.clearValueCount = 2;
    rpbi.pClearValues    = clears;

    vkCmdBeginRenderPass(f.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x      = 0;
    vp.y      = static_cast<float>(vp_.height);
    vp.width  = static_cast<float>(vp_.width);
    vp.height = -static_cast<float>(vp_.height);
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd, 0, 1, &vp);

    VkRect2D scissor{{0, 0}, {vp_.width, vp_.height}};
    vkCmdSetScissor(f.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &f.global_descriptor, 0, nullptr);

    auto mv = scene.view<Transform, MeshComponent>();
    for (auto entity : mv) {
        auto& tr = mv.get<Transform>(entity);
        auto& mc = mv.get<MeshComponent>(entity);
        if (!mc.mesh.valid() || mc.mesh.index >= meshes_.size()) continue;

        PushConstants pc{};
        pc.model = tr.matrix();
        vkCmdPushConstants(f.cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        VkDescriptorSet ms = default_material_.descriptor;
        if (mc.material.valid() && mc.material.index < materials_.size())
            ms = materials_[mc.material.index].descriptor;
        vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                                1, 1, &ms, 0, nullptr);

        auto& gm = meshes_[mc.mesh.index];
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(f.cmd, 0, 1, &gm.vertex_buffer.buffer, &off);
        vkCmdBindIndexBuffer(f.cmd, gm.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(f.cmd, gm.index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(f.cmd);
}

void EditorRenderer::begin_ui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void EditorRenderer::end_ui() {
    ImGui::Render();

    auto& f = frames_[current_frame_];
    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass  = ui_pass_;
    rpbi.framebuffer = ui_framebuffers_[image_index_];
    rpbi.renderArea  = {{0, 0}, swapchain_.extent};
    VkClearValue clear{};
    clear.color = {{0.04f, 0.04f, 0.05f, 1.0f}};
    rpbi.clearValueCount = 1;
    rpbi.pClearValues    = &clear;

    vkCmdBeginRenderPass(f.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), f.cmd);
    vkCmdEndRenderPass(f.cmd);
}

void EditorRenderer::end_frame() {
    auto& f = frames_[current_frame_];
    VK_CHECK(vkEndCommandBuffer(f.cmd));

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &f.image_available;
    si.pWaitDstStageMask    = &wait;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &f.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &f.render_finished;
    VK_CHECK(vkQueueSubmit(ctx_.graphics_queue, 1, &si, f.fence));

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &f.render_finished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain_.handle;
    pi.pImageIndices      = &image_index_;

    VkResult res = vkQueuePresentKHR(ctx_.present_queue, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || window_->framebuffer_resized()) {
        window_->reset_resize_flag();
        recreate_swapchain();
    }

    current_frame_ = (current_frame_ + 1) % static_cast<u32>(frames_.size());
}

// ─── Resource upload ────────────────────────────────────────────────

MeshHandle EditorRenderer::upload_mesh(const MeshData& data) {
    GPUMesh mesh;
    mesh.vertex_count = static_cast<u32>(data.vertices.size());
    mesh.index_count  = static_cast<u32>(data.indices.size());
    VkDeviceSize vb = data.vertices.size() * sizeof(Vertex);
    VkDeviceSize ib = data.indices.size() * sizeof(u32);

    mesh.vertex_buffer = create_buffer(ctx_.allocator, vb,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    mesh.index_buffer = create_buffer(ctx_.allocator, ib,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    upload_to_gpu(ctx_, command_pool_, mesh.vertex_buffer, data.vertices.data(), vb);
    upload_to_gpu(ctx_, command_pool_, mesh.index_buffer, data.indices.data(), ib);

    u32 idx = static_cast<u32>(meshes_.size());
    meshes_.push_back(mesh);
    return MeshHandle{idx};
}

TextureHandle EditorRenderer::load_texture(const std::string& path) {
    GPUTexture tex = load_texture_from_file(ctx_, command_pool_, path);
    u32 idx = static_cast<u32>(textures_.size());
    textures_.push_back(tex);
    return TextureHandle{idx};
}

MaterialHandle EditorRenderer::create_material(const MaterialData& data) {
    GPUMaterial mat;
    MaterialUBOData ud{};
    ud.base_color = data.base_color;
    ud.metallic   = data.metallic;
    ud.roughness  = data.roughness;
    ud.ao         = data.ao;

    mat.ubo = create_buffer(ctx_.allocator, sizeof(MaterialUBOData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    upload_buffer_data(ctx_.allocator, mat.ubo, &ud, sizeof(ud));

    mat.descriptor = desc_alloc_.allocate(ctx_.device, material_layout_);
    GPUTexture* tex = data.albedo_texture.valid() ? &textures_[data.albedo_texture.index] : &default_texture_;
    DescriptorWriter()
        .write_buffer(0, mat.ubo.buffer, sizeof(MaterialUBOData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .write_image(1, tex->view, tex->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .update(ctx_.device, mat.descriptor);

    u32 idx = static_cast<u32>(materials_.size());
    materials_.push_back(mat);
    return MaterialHandle{idx};
}

} // namespace lumios
