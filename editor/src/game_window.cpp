#include "game_window.h"
#include "graphics/vulkan/vk_pipeline.h"
#include "graphics/vulkan/vk_buffer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace lumios::editor {

bool GameWindow::open(VulkanContext& shared_ctx, const std::string& shader_dir,
                      const std::vector<GPUMesh>& meshes, const std::vector<GPUMaterial>& materials,
                      const GPUMaterial& default_mat) {
    ctx_ = &shared_ctx;
    meshes_ptr_      = &meshes;
    materials_ptr_   = &materials;
    default_mat_ptr_ = &default_mat;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(1280, 720, "Lumios Game", nullptr, nullptr);
    if (!window_) { LOG_ERROR("Failed to create game window"); return false; }

    VkResult res = glfwCreateWindowSurface(ctx_->instance, window_, nullptr, &surface_);
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to create game surface: %d", static_cast<int>(res));
        glfwDestroyWindow(window_); window_ = nullptr;
        return false;
    }

    vkDeviceWaitIdle(ctx_->device);

    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    if (!swapchain_.create(*ctx_, w, h, VK_NULL_HANDLE, surface_)) {
        LOG_ERROR("Failed to create game swapchain");
        vkDestroySurfaceKHR(ctx_->instance, surface_, nullptr);
        glfwDestroyWindow(window_); window_ = nullptr;
        return false;
    }

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = ctx_->graphics_family;
    vkCreateCommandPool(ctx_->device, &pci, nullptr, &command_pool_);

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100}
    };
    auto span = std::span<VkDescriptorPoolSize>(pool_sizes, 2);
    desc_alloc_.init(ctx_->device, 200, span);

    global_layout_ = DescriptorLayoutBuilder()
        .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .add(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx_->device);
    material_layout_ = DescriptorLayoutBuilder()
        .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .add(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(ctx_->device);

    if (!create_render_pass()) return false;
    if (!create_framebuffers()) return false;
    if (!create_pipeline(shader_dir)) return false;
    if (!create_frame_resources()) return false;

    current_frame_ = 0;
    image_index_   = 0;
    need_swapchain_recreate_ = false;

    LOG_INFO("Game window opened");
    return true;
}

void GameWindow::close() {
    if (!window_) return;
    vkDeviceWaitIdle(ctx_->device);

    for (auto& f : frames_) {
        if (f.fence)           { vkDestroyFence(ctx_->device, f.fence, nullptr); f.fence = VK_NULL_HANDLE; }
        if (f.render_finished) { vkDestroySemaphore(ctx_->device, f.render_finished, nullptr); f.render_finished = VK_NULL_HANDLE; }
        if (f.image_available) { vkDestroySemaphore(ctx_->device, f.image_available, nullptr); f.image_available = VK_NULL_HANDLE; }
        destroy_buffer(ctx_->allocator, f.global_ubo);
        destroy_buffer(ctx_->allocator, f.light_ubo);
        f.cmd = VK_NULL_HANDLE;
    }
    frames_.clear();

    desc_alloc_.destroy(ctx_->device);
    if (pipeline_)        { vkDestroyPipeline(ctx_->device, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (pipeline_layout_) { vkDestroyPipelineLayout(ctx_->device, pipeline_layout_, nullptr); pipeline_layout_ = VK_NULL_HANDLE; }
    if (material_layout_) { vkDestroyDescriptorSetLayout(ctx_->device, material_layout_, nullptr); material_layout_ = VK_NULL_HANDLE; }
    if (global_layout_)   { vkDestroyDescriptorSetLayout(ctx_->device, global_layout_, nullptr); global_layout_ = VK_NULL_HANDLE; }

    cleanup_framebuffers();
    if (render_pass_)     { vkDestroyRenderPass(ctx_->device, render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
    if (command_pool_)    { vkDestroyCommandPool(ctx_->device, command_pool_, nullptr); command_pool_ = VK_NULL_HANDLE; }
    swapchain_.cleanup(*ctx_);
    if (surface_)         { vkDestroySurfaceKHR(ctx_->instance, surface_, nullptr); surface_ = VK_NULL_HANDLE; }

    glfwDestroyWindow(window_);
    window_ = nullptr;
    images_in_flight_.clear();
    LOG_INFO("Game window closed");
}

bool GameWindow::should_close() const {
    return window_ && glfwWindowShouldClose(window_);
}

Camera GameWindow::resolve_game_camera(Scene& scene) {
    Camera cam;
    auto view = scene.view<Transform, CameraComponent>();
    for (auto entity : view) {
        auto& cc = view.get<CameraComponent>(entity);
        if (cc.primary) {
            auto& t = view.get<Transform>(entity);
            cam.set_position(t.position);

            glm::vec3 dir;
            dir.x = cos(glm::radians(t.rotation.y)) * cos(glm::radians(t.rotation.x));
            dir.y = sin(glm::radians(t.rotation.x));
            dir.z = sin(glm::radians(t.rotation.y)) * cos(glm::radians(t.rotation.x));
            glm::vec3 target = t.position + glm::normalize(dir);
            cam.look_at(target);

            int w, h;
            glfwGetFramebufferSize(window_, &w, &h);
            cam.set_perspective(cc.fov, static_cast<float>(w) / std::max(static_cast<float>(h), 1.0f),
                                cc.near_plane, cc.far_plane);
            return cam;
        }
    }
    cam.set_position({0, 5, 10});
    cam.look_at({0, 0, 0});
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    cam.set_aspect(static_cast<float>(w) / std::max(static_cast<float>(h), 1.0f));
    return cam;
}

bool GameWindow::create_render_pass() {
    VkAttachmentDescription atts[2]{};
    atts[0].format         = swapchain_.image_format;
    atts[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    atts[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    atts[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 2;
    ci.pAttachments    = atts;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;
    VK_CHECK(vkCreateRenderPass(ctx_->device, &ci, nullptr, &render_pass_));
    return render_pass_ != VK_NULL_HANDLE;
}

bool GameWindow::create_framebuffers() {
    framebuffers_.resize(swapchain_.image_views.size());
    for (size_t i = 0; i < swapchain_.image_views.size(); i++) {
        VkImageView views[] = {swapchain_.image_views[i], swapchain_.depth_view};
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass      = render_pass_;
        ci.attachmentCount = 2;
        ci.pAttachments    = views;
        ci.width           = swapchain_.extent.width;
        ci.height          = swapchain_.extent.height;
        ci.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(ctx_->device, &ci, nullptr, &framebuffers_[i]));
    }
    return true;
}

void GameWindow::cleanup_framebuffers() {
    for (auto fb : framebuffers_) vkDestroyFramebuffer(ctx_->device, fb, nullptr);
    framebuffers_.clear();
}

bool GameWindow::create_pipeline(const std::string& shader_dir) {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.size       = sizeof(PushConstants);

    VkDescriptorSetLayout layouts[] = {global_layout_, material_layout_};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount         = 2;
    li.pSetLayouts            = layouts;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges    = &push;
    VK_CHECK(vkCreatePipelineLayout(ctx_->device, &li, nullptr, &pipeline_layout_));

    auto vert = load_shader_module(ctx_->device, shader_dir + "/mesh.vert.spv");
    auto frag = load_shader_module(ctx_->device, shader_dir + "/mesh.frag.spv");
    if (!vert || !frag) return false;

    pipeline_ = PipelineBuilder()
        .set_shaders(vert, frag)
        .set_vertex_layout()
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .enable_depth_test(true, VK_COMPARE_OP_LESS)
        .disable_blending()
        .set_layout(pipeline_layout_)
        .build(ctx_->device, render_pass_);

    vkDestroyShaderModule(ctx_->device, vert, nullptr);
    vkDestroyShaderModule(ctx_->device, frag, nullptr);
    return pipeline_ != VK_NULL_HANDLE;
}

bool GameWindow::create_frame_resources() {
    u32 count = static_cast<u32>(swapchain_.images.size());
    frames_.resize(count);
    images_in_flight_.assign(count, VK_NULL_HANDLE);

    for (u32 i = 0; i < count; i++) {
        auto& f = frames_[i];
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool        = command_pool_;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(ctx_->device, &ai, &f.cmd) != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate game command buffer"); return false;
        }

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (vkCreateSemaphore(ctx_->device, &sci, nullptr, &f.image_available) != VK_SUCCESS ||
            vkCreateSemaphore(ctx_->device, &sci, nullptr, &f.render_finished) != VK_SUCCESS) {
            LOG_ERROR("Failed to create game semaphores"); return false;
        }

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(ctx_->device, &fci, nullptr, &f.fence) != VK_SUCCESS) {
            LOG_ERROR("Failed to create game fence"); return false;
        }

        f.global_ubo = create_buffer(ctx_->allocator, sizeof(GlobalUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        f.light_ubo = create_buffer(ctx_->allocator, sizeof(LightUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        f.global_descriptor = desc_alloc_.allocate(ctx_->device, global_layout_);
        DescriptorWriter()
            .write_buffer(0, f.global_ubo.buffer, sizeof(GlobalUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .write_buffer(1, f.light_ubo.buffer, sizeof(LightUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .update(ctx_->device, f.global_descriptor);
    }
    return true;
}

void GameWindow::recreate_swapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) { glfwGetFramebufferSize(window_, &w, &h); glfwWaitEvents(); }

    vkDeviceWaitIdle(ctx_->device);
    cleanup_framebuffers();

    auto old = swapchain_.handle;
    swapchain_.handle = VK_NULL_HANDLE;
    if (swapchain_.depth_view)  vkDestroyImageView(ctx_->device, swapchain_.depth_view, nullptr);
    if (swapchain_.depth_image) vmaDestroyImage(ctx_->allocator, swapchain_.depth_image, swapchain_.depth_allocation);
    for (auto v : swapchain_.image_views) vkDestroyImageView(ctx_->device, v, nullptr);
    swapchain_.image_views.clear();
    swapchain_.images.clear();

    swapchain_.create(*ctx_, w, h, old, surface_);
    if (old) vkDestroySwapchainKHR(ctx_->device, old, nullptr);

    images_in_flight_.assign(swapchain_.images.size(), VK_NULL_HANDLE);
    create_framebuffers();
    need_swapchain_recreate_ = false;
}

void GameWindow::render_frame(Scene& scene, ScriptManager* scripts, float dt) {
    if (!window_ || glfwWindowShouldClose(window_) || frames_.empty()) return;

    int fw = 0, fh = 0;
    glfwGetFramebufferSize(window_, &fw, &fh);
    if (fw == 0 || fh == 0) return;

    if (need_swapchain_recreate_) {
        recreate_swapchain();
        return;
    }

    if (scripts) scripts->update(dt);

    Camera cam = resolve_game_camera(scene);
    auto& f = frames_[current_frame_];

    vkWaitForFences(ctx_->device, 1, &f.fence, VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(ctx_->device, swapchain_.handle, UINT64_MAX,
                                          f.image_available, VK_NULL_HANDLE, &image_index_);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("vkAcquireNextImageKHR failed: %d", static_cast<int>(res));
        return;
    }

    bool suboptimal = (res == VK_SUBOPTIMAL_KHR);

    if (image_index_ >= static_cast<u32>(images_in_flight_.size())) {
        LOG_ERROR("image_index_ %u out of range (%u)", image_index_,
                  static_cast<u32>(images_in_flight_.size()));
        return;
    }

    if (images_in_flight_[image_index_] != VK_NULL_HANDLE)
        vkWaitForFences(ctx_->device, 1, &images_in_flight_[image_index_], VK_TRUE, UINT64_MAX);
    images_in_flight_[image_index_] = f.fence;

    vkResetFences(ctx_->device, 1, &f.fence);
    vkResetCommandBuffer(f.cmd, 0);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(f.cmd, &bi);

    GlobalUBO global{};
    global.view          = cam.view();
    global.projection    = cam.projection();
    global.camera_pos    = glm::vec4(cam.position(), 1.0f);
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

    upload_buffer_data(ctx_->allocator, f.global_ubo, &global, sizeof(global));
    upload_buffer_data(ctx_->allocator, f.light_ubo, &light_data, sizeof(light_data));

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass  = render_pass_;
    rpbi.framebuffer = framebuffers_[image_index_];
    rpbi.renderArea  = {{0, 0}, swapchain_.extent};
    VkClearValue clears[2];
    clears[0].color        = {{0.02f, 0.02f, 0.03f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};
    rpbi.clearValueCount = 2;
    rpbi.pClearValues    = clears;

    vkCmdBeginRenderPass(f.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.y      = static_cast<float>(swapchain_.extent.height);
    vp.width  = static_cast<float>(swapchain_.extent.width);
    vp.height = -static_cast<float>(swapchain_.extent.height);
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd, 0, 1, &vp);

    VkRect2D scissor{{0, 0}, swapchain_.extent};
    vkCmdSetScissor(f.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &f.global_descriptor, 0, nullptr);

    auto mv = scene.view<Transform, MeshComponent>();
    for (auto entity : mv) {
        auto& tr = mv.get<Transform>(entity);
        auto& mc = mv.get<MeshComponent>(entity);
        if (!mc.mesh.valid() || mc.mesh.index >= meshes_ptr_->size()) continue;

        PushConstants pc{};
        pc.model = tr.matrix();
        vkCmdPushConstants(f.cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        VkDescriptorSet ms = default_mat_ptr_->descriptor;
        if (mc.material.valid() && mc.material.index < materials_ptr_->size())
            ms = (*materials_ptr_)[mc.material.index].descriptor;
        vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                                1, 1, &ms, 0, nullptr);

        auto& gm = (*meshes_ptr_)[mc.mesh.index];
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(f.cmd, 0, 1, &gm.vertex_buffer.buffer, &off);
        vkCmdBindIndexBuffer(f.cmd, gm.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(f.cmd, gm.index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(f.cmd);
    vkEndCommandBuffer(f.cmd);

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &f.image_available;
    si.pWaitDstStageMask    = &wait;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &f.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &f.render_finished;
    vkQueueSubmit(ctx_->graphics_queue, 1, &si, f.fence);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &f.render_finished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain_.handle;
    pi.pImageIndices      = &image_index_;

    res = vkQueuePresentKHR(ctx_->present_queue, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || suboptimal)
        need_swapchain_recreate_ = true;

    current_frame_ = (current_frame_ + 1) % static_cast<u32>(frames_.size());
}

} // namespace lumios::editor
