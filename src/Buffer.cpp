
#include "pch.h"

#include "VkRay/Buffer.h"
#include "VkRay/VkRay_device.h"

// FORWARD DECLARATIONS ================================================================================================


namespace vr {

    // CONSTANTS =======================================================================================================

    // MACROS ==========================================================================================================

    // TYPES ===========================================================================================================

    // STATIC VARIABLES ================================================================================================

    // FUNCTION IMPLEMENTATION =========================================================================================

    uint32_t AlignUp(uint32_t value, uint32_t alignment)            { return (value + alignment - 1) & ~(alignment - 1); }


    uint64_t AlignUp(uint64_t value, uint64_t alignment)            { return (value + alignment - 1) & ~(alignment - 1); }

    // CLASS IMPLEMENTATION ============================================================================================

    // CLASS PUBLIC ====================================================================================================

    AllocatedImage vk_ray_device::create_image(const vk::ImageCreateInfo& imgInfo, VmaAllocationCreateFlags flags, VmaPool pool) {

        AllocatedImage out_image = {};
        VmaAllocationCreateInfo alloc_inf = {};
        alloc_inf.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc_inf.flags = flags;
        alloc_inf.pool = pool == nullptr ? m_current_pool : pool;

        VmaAllocationInfo allocationInfo = {};

        auto result = (vk::Result)vmaCreateImage(m_vma_allocator, (VkImageCreateInfo*)&imgInfo, &alloc_inf, (VkImage*)&out_image.Image, &out_image.Allocation, &allocationInfo);
        if (result != vk::Result::eSuccess)
            VR_LOG(error, "Failed to create Image: %s", vk::to_string(result));

        out_image.Size = allocationInfo.size;
        out_image.Width = imgInfo.extent.width;
        out_image.Height = imgInfo.extent.height;
        return out_image;
    }

    allocated_buffer vk_ray_device::create_buffer(vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, VmaAllocationCreateFlags flags, uint32_t alignment, VmaPool pool) {

        allocated_buffer outBuffer = {};
        VmaAllocationCreateInfo alloc_inf = {};
        alloc_inf.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        alloc_inf.flags = flags;
        alloc_inf.pool = pool == nullptr ? m_current_pool : pool;

        vk::BufferCreateInfo bufInfo = {};
        bufInfo.setSize(size);
        bufInfo.setUsage(bufferUsage | vk::BufferUsageFlagBits::eShaderDeviceAddress);

        vk::Result result;
        if (alignment)
            result = (vk::Result)vmaCreateBufferWithAlignment(m_vma_allocator, (VkBufferCreateInfo*)& bufInfo, &alloc_inf, // type punning
                alignment, (VkBuffer*)&outBuffer.Buffer, &outBuffer.Allocation, nullptr);
        else
            result = (vk::Result)vmaCreateBuffer(m_vma_allocator, (VkBufferCreateInfo*)& bufInfo, &alloc_inf, (VkBuffer*)&outBuffer.Buffer, &outBuffer.Allocation, nullptr);

        if (result != vk::Result::eSuccess)
        {
            VR_LOG(error, "Failed to create buffer: %s", vk::to_string(result));
            return outBuffer;
        }

        outBuffer.DevAddress = m_device.getBufferAddress(vk::BufferDeviceAddressInfo().setBuffer(outBuffer.Buffer));
        outBuffer.Size = size;
        return outBuffer;
    }


    allocated_buffer vk_ray_device::CreateInstanceBuffer(uint32_t instanceCount) {

        return create_buffer(instanceCount * sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }


    allocated_buffer vk_ray_device::CreateScratchBuffer(uint32_t size) {

        return create_buffer(size, vk::BufferUsageFlagBits::eStorageBuffer, 0, m_accel_properties.minAccelerationStructureScratchOffsetAlignment);
    }


    DescriptorBuffer vk_ray_device::CreateDescriptorBuffer(vk::DescriptorSetLayout layout, std::vector<DescriptorItem> &items, DescriptorBufferType type, uint32_t setCount) {

        DescriptorBuffer outBuffer = {};
        vk::BufferUsageFlags usageFlags = (vk::BufferUsageFlagBits)type; // DescriptorBufferType is a vulkan buffer usage flag enum
        vk::DeviceSize size = m_device.getDescriptorSetLayoutSizeEXT(layout, m_dyn_loader);
        size = AlignUp(size, m_descriptor_buffer_properties.descriptorBufferOffsetAlignment);

        // create a buffer that is big enough to hold all the descriptor sets and with the proper alignment
        outBuffer.Buffer = create_buffer(size * setCount, usageFlags, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            m_descriptor_buffer_properties.descriptorBufferOffsetAlignment);

        // fill the offsets to the items
        for (auto &item : items)
        {
            uint32_t offset = m_device.getDescriptorSetLayoutBindingOffsetEXT(layout, item.Binding, m_dyn_loader);
            item.BindingOffset = offset;
        }

        outBuffer.SetCount = setCount;
        outBuffer.SingleDescriptorSize = size;
        outBuffer.Type = type;

        return outBuffer;
    }


    void vk_ray_device::DestroyBuffer(allocated_buffer &buffer) {

        vmaDestroyBuffer(m_vma_allocator, buffer.Buffer, buffer.Allocation);
        buffer.Buffer = nullptr;
        buffer.Allocation = nullptr;
        buffer.DevAddress = 0;
    }


    void vk_ray_device::DestroyImage(AllocatedImage &img) {

        vmaDestroyImage(m_vma_allocator, img.Image, img.Allocation);
        img.Image = nullptr;
        img.Allocation = nullptr;
    }


    void vk_ray_device::UpdateBuffer(allocated_buffer alloc, void *data, const vk::DeviceSize size, uint32_t offset) {

        void *mappedData;
        vmaMapMemory(m_vma_allocator, alloc.Allocation, &mappedData);
        memcpy((uint8_t *)mappedData + offset, data, size);
        vmaUnmapMemory(m_vma_allocator, alloc.Allocation);
    }


    void vk_ray_device::CopyData(allocated_buffer src, allocated_buffer dst, vk::DeviceSize size, vk::CommandBuffer cmdBuf) {

        auto copyRegion = vk::BufferCopy().setSize(size);
        cmdBuf.copyBuffer(src.Buffer, dst.Buffer, copyRegion);
    }


    void *vk_ray_device::MapBuffer(allocated_buffer &buffer) {

        void *mappedData;
        vmaMapMemory(m_vma_allocator, buffer.Allocation, &mappedData);
        return mappedData;
    }


    void vk_ray_device::UnmapBuffer(allocated_buffer &buffer)         { vmaUnmapMemory(m_vma_allocator, buffer.Allocation); }


    void vk_ray_device::transition_image_layout(vk::CommandBuffer cmdBuf, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        const vk::ImageSubresourceRange& range, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {

        // transition image layout with ..set functions
        auto barrier = vk::ImageMemoryBarrier()
            .setOldLayout(oldLayout)
            .setNewLayout(newLayout)
            .setImage(image)
            .setSubresourceRange(range);

        // set src and dst access masks
        switch (oldLayout) {
            case vk::ImageLayout::eUndefined:                       barrier.setSrcAccessMask((vk::AccessFlagBits)0); break;
            case vk::ImageLayout::ePreinitialized:                  barrier.setSrcAccessMask(vk::AccessFlagBits::eHostWrite); break;
            case vk::ImageLayout::eTransferDstOptimal:              barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite); break;
            case vk::ImageLayout::eTransferSrcOptimal:              barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead); break;
            case vk::ImageLayout::eColorAttachmentOptimal:          barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite); break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:   barrier.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite); break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:           barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead); break;
            default: break;
        }
        // set dst access masks
        switch (newLayout) {
            case vk::ImageLayout::eTransferDstOptimal:              barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite); break;
            case vk::ImageLayout::eTransferSrcOptimal:              barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead); break;
            case vk::ImageLayout::eColorAttachmentOptimal:          barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite); break;
            case vk::ImageLayout::eDepthStencilAttachmentOptimal:   barrier.setDstAccessMask(barrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite); break;
            case vk::ImageLayout::eShaderReadOnlyOptimal:
                if (barrier.srcAccessMask == (vk::AccessFlagBits)0)
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite);
                barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
                break;
            default: break;
        }
        cmdBuf.pipelineBarrier(srcStage, dstStage, (vk::DependencyFlagBits)0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // CLASS PROTECTED =================================================================================================

    // CLASS PRIVATE ===================================================================================================

}
