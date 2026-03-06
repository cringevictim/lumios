#include "vk_descriptors.h"

namespace lumios {

// --- DescriptorLayoutBuilder ---

DescriptorLayoutBuilder& DescriptorLayoutBuilder::add(u32 binding, VkDescriptorType type,
                                                      VkShaderStageFlags stages, u32 count) {
    VkDescriptorSetLayoutBinding b{};
    b.binding         = binding;
    b.descriptorType  = type;
    b.descriptorCount = count;
    b.stageFlags      = stages;
    bindings_.push_back(b);
    return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device) {
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = static_cast<u32>(bindings_.size());
    ci.pBindings    = bindings_.data();

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout));
    return layout;
}

// --- DescriptorAllocator ---

void DescriptorAllocator::init(VkDevice device, u32 max_sets, std::span<VkDescriptorPoolSize> sizes) {
    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets       = max_sets;
    ci.poolSizeCount = static_cast<u32>(sizes.size());
    ci.pPoolSizes    = sizes.data();
    VK_CHECK(vkCreateDescriptorPool(device, &ci, nullptr, &pool_));
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &layout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device, &ai, &set));
    return set;
}

void DescriptorAllocator::reset(VkDevice device) {
    vkResetDescriptorPool(device, pool_, 0);
}

void DescriptorAllocator::destroy(VkDevice device) {
    if (pool_) { vkDestroyDescriptorPool(device, pool_, nullptr); pool_ = VK_NULL_HANDLE; }
}

// --- DescriptorWriter ---

DescriptorWriter& DescriptorWriter::write_buffer(u32 binding, VkBuffer buffer, VkDeviceSize size,
                                                  VkDeviceSize offset, VkDescriptorType type) {
    buffer_infos_.push_back({buffer, offset, size});

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstBinding      = binding;
    w.descriptorCount = 1;
    w.descriptorType  = type;
    writes_.push_back(w);
    return *this;
}

DescriptorWriter& DescriptorWriter::write_image(u32 binding, VkImageView view,
                                                 VkSampler sampler, VkImageLayout layout) {
    image_infos_.push_back({sampler, view, layout});

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstBinding      = binding;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes_.push_back(w);
    return *this;
}

void DescriptorWriter::update(VkDevice device, VkDescriptorSet set) {
    u32 buf_idx = 0, img_idx = 0;
    for (auto& w : writes_) {
        w.dstSet = set;
        if (w.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            w.pImageInfo = &image_infos_[img_idx++];
        } else {
            w.pBufferInfo = &buffer_infos_[buf_idx++];
        }
    }
    vkUpdateDescriptorSets(device, static_cast<u32>(writes_.size()), writes_.data(), 0, nullptr);
}

} // namespace lumios
