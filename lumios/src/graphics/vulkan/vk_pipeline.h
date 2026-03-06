#pragma once

#include "vk_common.h"
#include <vector>
#include <string>

namespace lumios {

VkShaderModule load_shader_module(VkDevice device, const std::string& path);

class PipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo>   shader_stages_;
    VkPipelineVertexInputStateCreateInfo            vertex_input_{};
    VkPipelineInputAssemblyStateCreateInfo          input_assembly_{};
    VkPipelineRasterizationStateCreateInfo          rasterizer_{};
    VkPipelineMultisampleStateCreateInfo            multisampling_{};
    VkPipelineDepthStencilStateCreateInfo           depth_stencil_{};
    VkPipelineColorBlendAttachmentState             blend_attachment_{};
    VkPipelineLayout                                layout_ = VK_NULL_HANDLE;

    std::vector<VkVertexInputBindingDescription>    bindings_;
    std::vector<VkVertexInputAttributeDescription>  attributes_;

public:
    PipelineBuilder();

    PipelineBuilder& set_shaders(VkShaderModule vert, VkShaderModule frag);
    PipelineBuilder& set_vertex_layout();
    PipelineBuilder& set_topology(VkPrimitiveTopology topo);
    PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
    PipelineBuilder& set_cull_mode(VkCullModeFlags cull, VkFrontFace front);
    PipelineBuilder& enable_depth_test(bool write, VkCompareOp op);
    PipelineBuilder& enable_blending_alpha();
    PipelineBuilder& disable_blending();
    PipelineBuilder& set_layout(VkPipelineLayout layout);

    VkPipeline build(VkDevice device, VkRenderPass pass);
};

} // namespace lumios
