#include "vk_renderer.h"
#include "vk_pipeline.h"
#include "vk_buffer.h"
#include "vk_texture.h"
#include "../../platform/window.h"
#include "../../scene/scene.h"
#include "../../scene/components.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace lumios {

// --- Renderer factory ---

Unique<Renderer> Renderer::create() {
    return std::make_unique<VulkanRenderer>();
}

// --- Init / shutdown ---

bool VulkanRenderer::init(Window& window, const std::string& shader_dir) {
    window_ = &window;
    shader_dir_ = shader_dir;

    if (!ctx_.init(window.handle())) return false;

    int w, h;
    window.get_framebuffer_size(w, h);
    if (!swapchain_.create(ctx_, w, h)) return false;
    images_in_flight_.resize(swapchain_.images.size(), VK_NULL_HANDLE);
    if (!create_render_pass()) return false;
    if (!create_framebuffers()) return false;
    if (!create_descriptors()) return false;
    if (!create_pipeline()) return false;
    if (!create_frame_resources()) return false;
    if (!create_default_resources()) return false;

    LOG_INFO("Vulkan renderer initialized");
    return true;
}

void VulkanRenderer::shutdown() {
    vkDeviceWaitIdle(ctx_.device);

    destroy_texture(ctx_, default_texture_);
    destroy_buffer(ctx_.allocator, default_material_.ubo);

    for (auto& m : materials_) destroy_buffer(ctx_.allocator, m.ubo);
    for (auto& t : textures_) destroy_texture(ctx_, t);
    for (auto& m : meshes_) {
        destroy_buffer(ctx_.allocator, m.vertex_buffer);
        destroy_buffer(ctx_.allocator, m.index_buffer);
    }

    for (auto& f : frames_) {
        destroy_buffer(ctx_.allocator, f.global_ubo);
        destroy_buffer(ctx_.allocator, f.light_ubo);
        vkDestroyFence(ctx_.device, f.in_flight, nullptr);
        vkDestroySemaphore(ctx_.device, f.render_finished, nullptr);
        vkDestroySemaphore(ctx_.device, f.image_available, nullptr);
        vkDestroyCommandPool(ctx_.device, f.command_pool, nullptr);
    }

    descriptor_alloc_.destroy(ctx_.device);
    if (pipeline_)        vkDestroyPipeline(ctx_.device, pipeline_, nullptr);
    if (pipeline_layout_) vkDestroyPipelineLayout(ctx_.device, pipeline_layout_, nullptr);
    if (material_set_layout_) vkDestroyDescriptorSetLayout(ctx_.device, material_set_layout_, nullptr);
    if (global_set_layout_)   vkDestroyDescriptorSetLayout(ctx_.device, global_set_layout_, nullptr);

    cleanup_swapchain_dependent();
    swapchain_.cleanup(ctx_);
    ctx_.shutdown();

    LOG_INFO("Vulkan renderer shut down");
}

// --- Render pass ---

bool VulkanRenderer::create_render_pass() {
    VkAttachmentDescription color_att{};
    color_att.format         = swapchain_.image_format;
    color_att.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_att{};
    depth_att.format         = swapchain_.depth_format;
    depth_att.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_att.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color_att, depth_att};
    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 2;
    ci.pAttachments    = attachments;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;

    VK_CHECK(vkCreateRenderPass(ctx_.device, &ci, nullptr, &render_pass_));
    return render_pass_ != VK_NULL_HANDLE;
}

bool VulkanRenderer::create_framebuffers() {
    framebuffers_.resize(swapchain_.image_views.size());
    for (size_t i = 0; i < swapchain_.image_views.size(); i++) {
        VkImageView views[] = {swapchain_.image_views[i], swapchain_.depth_view};
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = render_pass_;
        ci.attachmentCount = 2;
        ci.pAttachments    = views;
        ci.width           = swapchain_.extent.width;
        ci.height          = swapchain_.extent.height;
        ci.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(ctx_.device, &ci, nullptr, &framebuffers_[i]));
    }
    return true;
}

// --- Descriptors ---

bool VulkanRenderer::create_descriptors() {
    // Set 0: global UBO + light UBO
    global_set_layout_ = DescriptorLayoutBuilder()
        .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .add(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx_.device);

    // Set 1: material UBO + albedo sampler
    material_set_layout_ = DescriptorLayoutBuilder()
        .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .add(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx_.device);

    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 200},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100}
    };
    auto span = std::span<VkDescriptorPoolSize>(sizes, 2);
    descriptor_alloc_.init(ctx_.device, 200, span);

    return true;
}

// --- Pipeline ---

bool VulkanRenderer::create_pipeline() {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset     = 0;
    push.size       = sizeof(PushConstants);

    VkDescriptorSetLayout layouts[] = {global_set_layout_, material_set_layout_};

    VkPipelineLayoutCreateInfo li{};
    li.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    li.setLayoutCount         = 2;
    li.pSetLayouts            = layouts;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges    = &push;
    VK_CHECK(vkCreatePipelineLayout(ctx_.device, &li, nullptr, &pipeline_layout_));

    std::string vert_path = shader_dir_ + "/mesh.vert.spv";
    std::string frag_path = shader_dir_ + "/mesh.frag.spv";

    VkShaderModule vert_mod = load_shader_module(ctx_.device, vert_path);
    VkShaderModule frag_mod = load_shader_module(ctx_.device, frag_path);
    if (!vert_mod || !frag_mod) {
        LOG_ERROR("Failed to load shaders from %s", shader_dir_.c_str());
        return false;
    }

    pipeline_ = PipelineBuilder()
        .set_shaders(vert_mod, frag_mod)
        .set_vertex_layout()
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .enable_depth_test(true, VK_COMPARE_OP_LESS)
        .disable_blending()
        .set_layout(pipeline_layout_)
        .build(ctx_.device, render_pass_);

    vkDestroyShaderModule(ctx_.device, vert_mod, nullptr);
    vkDestroyShaderModule(ctx_.device, frag_mod, nullptr);

    LOG_INFO("Graphics pipeline created");
    return pipeline_ != VK_NULL_HANDLE;
}

// --- Frame resources ---

bool VulkanRenderer::create_frame_resources() {
    frame_count_ = static_cast<u32>(swapchain_.images.size());
    frames_.resize(frame_count_);
    for (auto& f : frames_) {
        VkCommandPoolCreateInfo pci{};
        pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = ctx_.graphics_family;
        VK_CHECK(vkCreateCommandPool(ctx_.device, &pci, nullptr, &f.command_pool));

        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = f.command_pool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(ctx_.device, &ai, &f.command_buffer));

        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(ctx_.device, &sci, nullptr, &f.image_available));
        VK_CHECK(vkCreateSemaphore(ctx_.device, &sci, nullptr, &f.render_finished));

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(ctx_.device, &fci, nullptr, &f.in_flight));

        f.global_ubo = create_buffer(ctx_.allocator, sizeof(GlobalUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        f.light_ubo = create_buffer(ctx_.allocator, sizeof(LightUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        f.global_descriptor = descriptor_alloc_.allocate(ctx_.device, global_set_layout_);

        DescriptorWriter()
            .write_buffer(0, f.global_ubo.buffer, sizeof(GlobalUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .write_buffer(1, f.light_ubo.buffer, sizeof(LightUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .update(ctx_.device, f.global_descriptor);
    }
    return true;
}

// --- Default resources ---

bool VulkanRenderer::create_default_resources() {
    default_texture_ = create_default_white_texture(ctx_, frames_[0].command_pool);

    MaterialUBOData mat_data{};
    mat_data.base_color = {1, 1, 1, 1};
    mat_data.metallic   = 0.0f;
    mat_data.roughness  = 0.5f;
    mat_data.ao         = 1.0f;

    default_material_.ubo = create_buffer(ctx_.allocator, sizeof(MaterialUBOData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    upload_buffer_data(ctx_.allocator, default_material_.ubo, &mat_data, sizeof(mat_data));

    default_material_.descriptor = descriptor_alloc_.allocate(ctx_.device, material_set_layout_);
    DescriptorWriter()
        .write_buffer(0, default_material_.ubo.buffer, sizeof(MaterialUBOData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .write_image(1, default_texture_.view, default_texture_.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .update(ctx_.device, default_material_.descriptor);

    return true;
}

// --- Swapchain management ---

void VulkanRenderer::cleanup_swapchain_dependent() {
    for (auto fb : framebuffers_) vkDestroyFramebuffer(ctx_.device, fb, nullptr);
    framebuffers_.clear();
    if (render_pass_) { vkDestroyRenderPass(ctx_.device, render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
}

void VulkanRenderer::recreate_swapchain() {
    int w = 0, h = 0;
    window_->get_framebuffer_size(w, h);
    while (w == 0 || h == 0) {
        window_->get_framebuffer_size(w, h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx_.device);
    cleanup_swapchain_dependent();

    auto old = swapchain_.handle;
    swapchain_.handle = VK_NULL_HANDLE;

    // Cleanup old resources except handle (passed as old swapchain)
    if (swapchain_.depth_view)  vkDestroyImageView(ctx_.device, swapchain_.depth_view, nullptr);
    if (swapchain_.depth_image) vmaDestroyImage(ctx_.allocator, swapchain_.depth_image, swapchain_.depth_allocation);
    for (auto v : swapchain_.image_views) vkDestroyImageView(ctx_.device, v, nullptr);
    swapchain_.image_views.clear();
    swapchain_.images.clear();

    swapchain_.create(ctx_, w, h, old);
    if (old) vkDestroySwapchainKHR(ctx_.device, old, nullptr);

    images_in_flight_.clear();
    images_in_flight_.resize(swapchain_.images.size(), VK_NULL_HANDLE);

    create_render_pass();
    create_framebuffers();
}

// --- Frame lifecycle ---

bool VulkanRenderer::begin_frame() {
    auto& f = frames_[current_frame_];
    vkWaitForFences(ctx_.device, 1, &f.in_flight, VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(ctx_.device, swapchain_.handle, UINT64_MAX,
                                            f.image_available, VK_NULL_HANDLE, &image_index_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return false;
    }

    // Wait if a previous frame is still using this swapchain image
    if (images_in_flight_[image_index_] != VK_NULL_HANDLE) {
        vkWaitForFences(ctx_.device, 1, &images_in_flight_[image_index_], VK_TRUE, UINT64_MAX);
    }
    images_in_flight_[image_index_] = f.in_flight;

    vkResetFences(ctx_.device, 1, &f.in_flight);
    vkResetCommandBuffer(f.command_buffer, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(f.command_buffer, &bi));
    return true;
}

void VulkanRenderer::end_frame() {
    auto& f = frames_[current_frame_];
    VK_CHECK(vkEndCommandBuffer(f.command_buffer));

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &f.image_available;
    si.pWaitDstStageMask    = &wait_stage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &f.command_buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &f.render_finished;

    VK_CHECK(vkQueueSubmit(ctx_.graphics_queue, 1, &si, f.in_flight));

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &f.render_finished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain_.handle;
    pi.pImageIndices      = &image_index_;

    VkResult result = vkQueuePresentKHR(ctx_.present_queue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        window_->framebuffer_resized()) {
        window_->reset_resize_flag();
        recreate_swapchain();
    }

    current_frame_ = (current_frame_ + 1) % frame_count_;
}

// --- Resource upload ---

MeshHandle VulkanRenderer::upload_mesh(const MeshData& data) {
    GPUMesh mesh;
    mesh.vertex_count = static_cast<u32>(data.vertices.size());
    mesh.index_count  = static_cast<u32>(data.indices.size());

    VkDeviceSize vb_size = data.vertices.size() * sizeof(Vertex);
    VkDeviceSize ib_size = data.indices.size() * sizeof(u32);

    mesh.vertex_buffer = create_buffer(ctx_.allocator, vb_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh.index_buffer = create_buffer(ctx_.allocator, ib_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    upload_to_gpu(ctx_, frames_[0].command_pool, mesh.vertex_buffer, data.vertices.data(), vb_size);
    upload_to_gpu(ctx_, frames_[0].command_pool, mesh.index_buffer, data.indices.data(), ib_size);

    u32 idx = static_cast<u32>(meshes_.size());
    meshes_.push_back(mesh);
    return MeshHandle{idx};
}

TextureHandle VulkanRenderer::load_texture(const std::string& path) {
    GPUTexture tex = load_texture_from_file(ctx_, frames_[0].command_pool, path);
    u32 idx = static_cast<u32>(textures_.size());
    textures_.push_back(tex);
    return TextureHandle{idx};
}

MaterialHandle VulkanRenderer::create_material(const MaterialData& data) {
    GPUMaterial mat;

    MaterialUBOData ubo_data{};
    ubo_data.base_color = data.base_color;
    ubo_data.metallic   = data.metallic;
    ubo_data.roughness  = data.roughness;
    ubo_data.ao         = data.ao;

    mat.ubo = create_buffer(ctx_.allocator, sizeof(MaterialUBOData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    upload_buffer_data(ctx_.allocator, mat.ubo, &ubo_data, sizeof(ubo_data));

    mat.descriptor = descriptor_alloc_.allocate(ctx_.device, material_set_layout_);

    GPUTexture* tex = data.albedo_texture.valid() ? &textures_[data.albedo_texture.index] : &default_texture_;

    DescriptorWriter()
        .write_buffer(0, mat.ubo.buffer, sizeof(MaterialUBOData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .write_image(1, tex->view, tex->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .update(ctx_.device, mat.descriptor);

    u32 idx = static_cast<u32>(materials_.size());
    materials_.push_back(mat);
    return MaterialHandle{idx};
}

// --- Scene rendering ---

void VulkanRenderer::render_scene(Scene& scene, const Camera& camera) {
    auto& f = frames_[current_frame_];
    VkCommandBuffer cmd = f.command_buffer;

    // Update global UBO
    GlobalUBO global{};
    global.view        = camera.view();
    global.projection  = camera.projection();
    global.camera_pos  = glm::vec4(camera.position(), 1.0f);
    global.ambient_color = glm::vec4(0.08f, 0.08f, 0.12f, 0.3f);

    // Gather lights
    LightUBO light_data{};
    int light_count = 0;
    auto light_view = scene.view<Transform, LightComponent>();
    for (auto entity : light_view) {
        if (light_count >= 16) break;
        auto& t = light_view.get<Transform>(entity);
        auto& l = light_view.get<LightComponent>(entity);

        auto& gl = light_data.lights[light_count];
        gl.position  = glm::vec4(t.position, l.type == LightType::Directional ? 0.0f : 1.0f);
        gl.color     = glm::vec4(l.color, l.intensity);
        gl.direction = glm::vec4(glm::normalize(glm::vec3(
            cos(glm::radians(t.rotation.y)) * cos(glm::radians(t.rotation.x)),
            sin(glm::radians(t.rotation.x)),
            sin(glm::radians(t.rotation.y)) * cos(glm::radians(t.rotation.x))
        )), 0.0f);
        gl.params = glm::vec4(l.range, cos(glm::radians(l.spot_angle)), static_cast<float>(static_cast<int>(l.type)), 0.0f);
        light_count++;
    }
    global.num_lights = light_count;

    upload_buffer_data(ctx_.allocator, f.global_ubo, &global, sizeof(global));
    upload_buffer_data(ctx_.allocator, f.light_ubo, &light_data, sizeof(light_data));

    // Begin render pass
    VkRenderPassBeginInfo rpbi{};
    rpbi.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass  = render_pass_;
    rpbi.framebuffer = framebuffers_[image_index_];
    rpbi.renderArea  = {{0, 0}, swapchain_.extent};

    VkClearValue clears[2];
    clears[0].color = {{0.02f, 0.02f, 0.03f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};
    rpbi.clearValueCount = 2;
    rpbi.pClearValues    = clears;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // Negative viewport height flips Y for Vulkan (core since 1.1)
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = static_cast<float>(swapchain_.extent.height);
    vp.width    = static_cast<float>(swapchain_.extent.width);
    vp.height   = -static_cast<float>(swapchain_.extent.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{{0, 0}, swapchain_.extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind pipeline and global descriptors
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &f.global_descriptor, 0, nullptr);

    // Draw each mesh entity
    auto mesh_view = scene.view<Transform, MeshComponent>();
    for (auto entity : mesh_view) {
        auto& transform = mesh_view.get<Transform>(entity);
        auto& mc = mesh_view.get<MeshComponent>(entity);

        if (!mc.mesh.valid() || mc.mesh.index >= meshes_.size()) continue;
        auto& gpu_mesh = meshes_[mc.mesh.index];

        // Push model matrix
        PushConstants pc{};
        pc.model = transform.matrix();
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        // Bind material
        VkDescriptorSet mat_set = default_material_.descriptor;
        if (mc.material.valid() && mc.material.index < materials_.size())
            mat_set = materials_[mc.material.index].descriptor;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                                1, 1, &mat_set, 0, nullptr);

        // Draw
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &gpu_mesh.vertex_buffer.buffer, &offset);
        vkCmdBindIndexBuffer(cmd, gpu_mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, gpu_mesh.index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
}

} // namespace lumios
