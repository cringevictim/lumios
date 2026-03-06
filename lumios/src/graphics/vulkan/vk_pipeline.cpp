#include "vk_pipeline.h"
#include "../../graphics/gpu_types.h"
#include <fstream>

namespace lumios {

VkShaderModule load_shader_module(VkDevice device, const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader: %s", path.c_str());
        return VK_NULL_HANDLE;
    }

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<u32> code(size / sizeof(u32));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), size);

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode    = code.data();

    VkShaderModule mod;
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &mod));
    return mod;
}

PipelineBuilder::PipelineBuilder() {
    input_assembly_.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    rasterizer_.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_.lineWidth   = 1.0f;
    rasterizer_.cullMode    = VK_CULL_MODE_BACK_BIT;
    rasterizer_.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    multisampling_.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    depth_stencil_.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_.depthTestEnable  = VK_TRUE;
    depth_stencil_.depthWriteEnable = VK_TRUE;
    depth_stencil_.depthCompareOp   = VK_COMPARE_OP_LESS;

    blend_attachment_.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment_.blendEnable = VK_FALSE;

    vertex_input_.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
}

PipelineBuilder& PipelineBuilder::set_shaders(VkShaderModule vert, VkShaderModule frag) {
    shader_stages_.clear();
    VkPipelineShaderStageCreateInfo vi{};
    vi.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vi.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vi.module = vert;
    vi.pName  = "main";
    shader_stages_.push_back(vi);

    VkPipelineShaderStageCreateInfo fi{};
    fi.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fi.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fi.module = frag;
    fi.pName  = "main";
    shader_stages_.push_back(fi);
    return *this;
}

PipelineBuilder& PipelineBuilder::set_vertex_layout() {
    bindings_.clear();
    attributes_.clear();

    VkVertexInputBindingDescription bind{};
    bind.binding   = 0;
    bind.stride    = sizeof(Vertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings_.push_back(bind);

    // position
    attributes_.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    // normal
    attributes_.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    // uv
    attributes_.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
    // color
    attributes_.push_back({3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)});

    vertex_input_.vertexBindingDescriptionCount   = static_cast<u32>(bindings_.size());
    vertex_input_.pVertexBindingDescriptions       = bindings_.data();
    vertex_input_.vertexAttributeDescriptionCount  = static_cast<u32>(attributes_.size());
    vertex_input_.pVertexAttributeDescriptions     = attributes_.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::set_topology(VkPrimitiveTopology topo) {
    input_assembly_.topology = topo;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_polygon_mode(VkPolygonMode mode) {
    rasterizer_.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_cull_mode(VkCullModeFlags cull, VkFrontFace front) {
    rasterizer_.cullMode  = cull;
    rasterizer_.frontFace = front;
    return *this;
}

PipelineBuilder& PipelineBuilder::enable_depth_test(bool write, VkCompareOp op) {
    depth_stencil_.depthTestEnable  = VK_TRUE;
    depth_stencil_.depthWriteEnable = write ? VK_TRUE : VK_FALSE;
    depth_stencil_.depthCompareOp   = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::enable_blending_alpha() {
    blend_attachment_.blendEnable         = VK_TRUE;
    blend_attachment_.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_attachment_.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment_.colorBlendOp        = VK_BLEND_OP_ADD;
    blend_attachment_.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment_.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment_.alphaBlendOp        = VK_BLEND_OP_ADD;
    return *this;
}

PipelineBuilder& PipelineBuilder::disable_blending() {
    blend_attachment_.blendEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_layout(VkPipelineLayout layout) {
    layout_ = layout;
    return *this;
}

VkPipeline PipelineBuilder::build(VkDevice device, VkRenderPass pass) {
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blend_attachment_;

    std::vector<VkDynamicState> dynamics = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = static_cast<u32>(dynamics.size());
    dyn.pDynamicStates    = dynamics.data();

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.stageCount          = static_cast<u32>(shader_stages_.size());
    ci.pStages             = shader_stages_.data();
    ci.pVertexInputState   = &vertex_input_;
    ci.pInputAssemblyState = &input_assembly_;
    ci.pViewportState      = &viewport_state;
    ci.pRasterizationState = &rasterizer_;
    ci.pMultisampleState   = &multisampling_;
    ci.pDepthStencilState  = &depth_stencil_;
    ci.pColorBlendState    = &blend;
    ci.pDynamicState       = &dyn;
    ci.layout              = layout_;
    ci.renderPass          = pass;
    ci.subpass             = 0;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));
    return pipeline;
}

} // namespace lumios
