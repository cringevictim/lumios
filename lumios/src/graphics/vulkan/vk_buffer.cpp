#include "vk_buffer.h"
#include "vk_init.h"

namespace lumios {

GPUBuffer create_buffer(VmaAllocator allocator, VkDeviceSize size,
                        VkBufferUsageFlags usage, VmaMemoryUsage mem_usage) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = size;
    bci.usage = usage;

    VmaAllocationCreateInfo aci{};
    aci.usage = mem_usage;
    if (mem_usage == VMA_MEMORY_USAGE_CPU_ONLY || mem_usage == VMA_MEMORY_USAGE_CPU_TO_GPU)
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    GPUBuffer buf;
    buf.size = size;
    VK_CHECK(vmaCreateBuffer(allocator, &bci, &aci, &buf.buffer, &buf.allocation, nullptr));
    return buf;
}

void destroy_buffer(VmaAllocator allocator, GPUBuffer& buf) {
    if (buf.buffer) {
        vmaDestroyBuffer(allocator, buf.buffer, buf.allocation);
        buf.buffer = VK_NULL_HANDLE;
        buf.allocation = VK_NULL_HANDLE;
    }
}

void upload_buffer_data(VmaAllocator allocator, GPUBuffer& buf, const void* data, VkDeviceSize size) {
    void* mapped;
    vmaMapMemory(allocator, buf.allocation, &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(allocator, buf.allocation);
}

void upload_to_gpu(VulkanContext& ctx, VkCommandPool pool,
                   GPUBuffer& dst, const void* data, VkDeviceSize size) {
    GPUBuffer staging = create_buffer(ctx.allocator, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    upload_buffer_data(ctx.allocator, staging, data, size);

    VkCommandBuffer cmd = ctx.begin_single_command(pool);

    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer, dst.buffer, 1, &region);

    ctx.end_single_command(pool, cmd);

    destroy_buffer(ctx.allocator, staging);
}

} // namespace lumios
