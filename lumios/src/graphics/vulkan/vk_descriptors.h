#pragma once

#include "vk_common.h"
#include <vector>

namespace lumios {

class DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings_;
public:
    DescriptorLayoutBuilder& add(u32 binding, VkDescriptorType type,
                                 VkShaderStageFlags stages, u32 count = 1);
    VkDescriptorSetLayout build(VkDevice device);
};

class DescriptorAllocator {
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
public:
    void init(VkDevice device, u32 max_sets, std::span<VkDescriptorPoolSize> sizes);
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
    void reset(VkDevice device);
    void destroy(VkDevice device);
};

class DescriptorWriter {
    std::vector<VkDescriptorBufferInfo> buffer_infos_;
    std::vector<VkDescriptorImageInfo>  image_infos_;
    std::vector<VkWriteDescriptorSet>   writes_;
public:
    DescriptorWriter& write_buffer(u32 binding, VkBuffer buffer, VkDeviceSize size,
                                   VkDeviceSize offset, VkDescriptorType type);
    DescriptorWriter& write_image(u32 binding, VkImageView view, VkSampler sampler,
                                  VkImageLayout layout);
    void update(VkDevice device, VkDescriptorSet set);
};

} // namespace lumios
