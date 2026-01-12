
#include "pch.h"

#include "VkRay/SBT.h"
#include "VkRay/VkRay_device.h"
#include "VkRay/Buffer.h"

// FORWARD DECLARATIONS ================================================================================================


namespace vr {

    // CONSTANTS =======================================================================================================

    // MACROS ==========================================================================================================

    // TYPES ===========================================================================================================

    // STATIC VARIABLES ================================================================================================

    // FUNCTION IMPLEMENTATION =========================================================================================

    // CLASS IMPLEMENTATION ============================================================================================

    // CLASS PUBLIC ====================================================================================================

    std::vector<uint8_t> vk_ray_device::get_handles_for_sbtbuffer(vk::Pipeline pipeline, uint32_t firstGroup, uint32_t groupCount) {

        uint32_t alignedSize = AlignUp(m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        uint32_t size = alignedSize * groupCount;
        std::vector<uint8_t> handles(size);

        auto result = m_device.getRayTracingShaderGroupHandlesKHR(pipeline, firstGroup, groupCount, size, handles.data(), m_dyn_loader);
        if (result != vk::Result::eSuccess)
            VR_LOG(error, "get_handles_for_sbtbuffer: Failed to get ray tracing shader group handles");
        return handles;
    }


    void vk_ray_device::get_handles_for_sbtbuffer(vk::Pipeline pipeline, uint32_t firstGroup, uint32_t groupCount, void *data) {

        uint32_t alignedSize = AlignUp(m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        uint32_t size = alignedSize * groupCount;

        auto result = m_device.getRayTracingShaderGroupHandlesKHR(pipeline, firstGroup, groupCount, size, data, m_dyn_loader);
        if (result != vk::Result::eSuccess)
            VR_LOG(error, "get_handles_for_sbtbuffer: Failed to get ray tracing shader group handles");
    }


    void vk_ray_device::WriteToSBT(SBTBuffer sbtBuf, ShaderGroup group, uint32_t groupIndex, void *data, uint32_t dataSize, void *mappedData) {

        allocated_buffer* buffer = nullptr;
        vk::StridedDeviceAddressRegionKHR *addressRegion;
        switch (group) {

            case ShaderGroup::RayGen:
                buffer = &sbtBuf.RayGenBuffer;
                addressRegion = &sbtBuf.RayGenRegion;
                break;
            case ShaderGroup::Miss:
                buffer = &sbtBuf.MissBuffer;
                addressRegion = &sbtBuf.MissRegion;
                break;
            case ShaderGroup::HitGroup:
                buffer = &sbtBuf.HitGroupBuffer;
                addressRegion = &sbtBuf.HitGroupRegion;
                break;
            case ShaderGroup::Callable:
                buffer = &sbtBuf.CallableBuffer;
                addressRegion = &sbtBuf.CallableRegion;
                break;
        }
        if (!buffer) {

            VR_LOG(error, "WriteToSBT: Invalid shader group");
            return;
        }

        // Offset to the start of the requested group and apply the opaque handle size for the SBT
        uint32_t offset = (groupIndex * addressRegion->stride) + m_ray_tracing_properties.shaderGroupHandleSize;

        // make sure the data size is not too large
        if (offset + dataSize > addressRegion->size) {

            VR_LOG(error, "WriteToSBT: Data size is too large for shader group");
            return;
        }

        if (mappedData)
            memcpy((uint8_t *)mappedData + offset, data, dataSize);     // if we have a mapped buffer, just copy the data
        else
            UpdateBuffer(*buffer, data, dataSize, offset);              // else we update the buffer with the data
    }


    SBTBuffer vk_ray_device::CreateSBT(vk::Pipeline pipeline, const SBTInfo &sbt) {
        SBTBuffer outSBT;

        const uint32_t rgen_count = sbt.RayGenIndices.size();
        const uint32_t miss_count = sbt.MissIndices.size();
        const uint32_t hit_count = sbt.HitGroupIndices.size();
        const uint32_t call_count = sbt.CallableIndices.size();
        // const uint32_t groupsCount = rgen_count + miss_count + hit_count + call_count;
        const uint32_t handleSize = m_ray_tracing_properties.shaderGroupHandleSize;

        uint32_t rgen_size = AlignUp(sbt.RayGenShaderRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        uint32_t miss_size = AlignUp(sbt.MissShaderRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        uint32_t hit_size = AlignUp(sbt.HitGroupRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        uint32_t call_size = AlignUp(sbt.CallableShaderRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);

        // Create all buffers for the SBT
        if (sbt.RayGenIndices.size() || sbt.ReserveRayGenGroups)
            outSBT.RayGenBuffer = create_buffer(
                rgen_size * (rgen_count + sbt.ReserveRayGenGroups),
                vk::BufferUsageFlagBits::eShaderDeviceAddressKHR | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, m_ray_tracing_properties.shaderGroupBaseAlignment);

        if (sbt.MissIndices.size() || sbt.ReserveMissGroups)
            outSBT.MissBuffer = create_buffer(
                miss_size * (miss_count + sbt.ReserveMissGroups),
                vk::BufferUsageFlagBits::eShaderDeviceAddressKHR | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, m_ray_tracing_properties.shaderGroupBaseAlignment);

        if (sbt.HitGroupIndices.size() || sbt.ReserveHitGroups)
            outSBT.HitGroupBuffer = create_buffer(
                hit_size * (hit_count + sbt.ReserveHitGroups),
                vk::BufferUsageFlagBits::eShaderDeviceAddressKHR | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, m_ray_tracing_properties.shaderGroupBaseAlignment);

        if (sbt.CallableIndices.size() || sbt.ReserveCallableGroups)
            outSBT.CallableBuffer = create_buffer(
                call_size * (call_count + sbt.ReserveCallableGroups),
                vk::BufferUsageFlagBits::eShaderDeviceAddressKHR | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, m_ray_tracing_properties.shaderGroupBaseAlignment);

        // For filling the stride and size of the regions, we don't want to set stride when there is no shader of that
        // type. We didn't do this earlier because we needed to know the size of the shader group handles to reserve
        // space for them.
        if (rgen_count == 0)        rgen_size = 0;
        if (miss_count == 0)        miss_size = 0;
        if (hit_count == 0)         hit_size = 0;
        if (call_count == 0)        call_size = 0;

        // fill in offsets for all shader groups
        if (rgen_count > 0)
            outSBT.RayGenRegion = vk::StridedDeviceAddressRegionKHR()
                .setDeviceAddress(outSBT.RayGenBuffer.DevAddress)
                .setStride(rgen_size)
                .setSize(rgen_size * rgen_count);

        if (miss_count > 0)
            outSBT.MissRegion = vk::StridedDeviceAddressRegionKHR()
                .setDeviceAddress(outSBT.MissBuffer.DevAddress)
                .setStride(miss_size)
                .setSize(miss_size * miss_count);

        if (hit_count > 0)
            outSBT.HitGroupRegion = vk::StridedDeviceAddressRegionKHR()
                .setDeviceAddress(outSBT.HitGroupBuffer.DevAddress)
                .setStride(hit_size)
                .setSize(hit_size * hit_count);

        if (call_count > 0)
            outSBT.CallableRegion = vk::StridedDeviceAddressRegionKHR()
                .setDeviceAddress(outSBT.CallableBuffer.DevAddress)
                .setStride(call_size)
                .setSize(call_size * call_count);

        // const uint8_t* opaqueHandle = new uint8_t[handleSize];

        // copy records and shader handles into the SBT buffer
        uint8_t *rgenData = rgen_size > 0 ? (uint8_t *)MapBuffer(outSBT.RayGenBuffer) : nullptr;
        for (uint32_t i = 0; rgenData && i < rgen_count; i++)
        {
            uint32_t shaderIndex = sbt.RayGenIndices[i];
            const uint8_t *dest = rgenData + (i * rgen_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
            rgenData += rgen_size; // advance to the next record
        }
        uint8_t *missData = miss_size > 0 ? (uint8_t *)MapBuffer(outSBT.MissBuffer) : nullptr;
        for (uint32_t i = 0; missData && i < miss_count; i++)
        {
            uint32_t shaderIndex = sbt.MissIndices[i];
            const uint8_t *dest = missData + (i * miss_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
        }
        uint8_t *hitData = hit_size > 0 ? (uint8_t *)MapBuffer(outSBT.HitGroupBuffer) : nullptr;
        for (uint32_t i = 0; hitData && i < hit_count; i++)
        {
            uint32_t shaderIndex = sbt.HitGroupIndices[i];
            const uint8_t *dest = hitData + (i * hit_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
        }
        uint8_t *callData = call_size > 0 ? (uint8_t *)MapBuffer(outSBT.CallableBuffer) : nullptr;
        for (uint32_t i = 0; callData && i < call_count; i++)
        {
            uint32_t shaderIndex = sbt.CallableIndices[i];
            const uint8_t *dest = callData + (i * call_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
        }

        // unmap all the buffers
        if (rgenData)
            UnmapBuffer(outSBT.RayGenBuffer);
        if (missData)
            UnmapBuffer(outSBT.MissBuffer);
        if (hitData)
            UnmapBuffer(outSBT.HitGroupBuffer);
        if (callData)
            UnmapBuffer(outSBT.CallableBuffer);

        return outSBT;
    }

    bool vk_ray_device::RebuildSBT(vk::Pipeline pipeline, SBTBuffer &buffer, const SBTInfo &sbt)
    {
        if (!CanSBTFitShaders(buffer, sbt))
            return false;

        const uint32_t rgen_count = sbt.RayGenIndices.size();
        const uint32_t miss_count = sbt.MissIndices.size();
        const uint32_t hit_count = sbt.HitGroupIndices.size();
        const uint32_t call_count = sbt.CallableIndices.size();
        // const uint32_t groupsCount = rgen_count + miss_count + hit_count + call_count;

        const uint32_t handleSize = AlignUp(m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t rgen_size = AlignUp(sbt.RayGenShaderRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t miss_size = AlignUp(sbt.MissShaderRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t hit_size = AlignUp(sbt.HitGroupRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t call_size = AlignUp(sbt.CallableShaderRecordSize + handleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);

        // we have to rewrite opaque handles to all the groups in the SBT, because on some implementations just keeping
        // the old opaque handles and adding new opaque handles to the new added groups doesn't work

        // const uint8_t* opaqueHandle = new uint8_t[handleSize];

        // copy records and shader handles into the SBT buffer
        uint8_t *rgenData = rgen_size > 0 ? (uint8_t *)MapBuffer(buffer.RayGenBuffer) : nullptr;
        for (uint32_t i = 0; rgenData && i < rgen_count; i++)
        {
            uint32_t shaderIndex = sbt.RayGenIndices[i];
            const uint8_t *dest = rgenData + (i * rgen_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
            rgenData += rgen_size; // advance to the next record
        }
        uint8_t *missData = miss_size > 0 ? (uint8_t *)MapBuffer(buffer.MissBuffer) : nullptr;
        for (uint32_t i = 0; missData && i < miss_count; i++)
        {
            uint32_t shaderIndex = sbt.MissIndices[i];
            const uint8_t *dest = missData + (i * miss_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
        }
        uint8_t *hitData = hit_size > 0 ? (uint8_t *)MapBuffer(buffer.HitGroupBuffer) : nullptr;
        for (uint32_t i = 0; hitData && i < hit_count; i++)
        {
            uint32_t shaderIndex = sbt.HitGroupIndices[i];
            const uint8_t *dest = hitData + (i * hit_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
        }
        uint8_t *callData = call_size > 0 ? (uint8_t *)MapBuffer(buffer.CallableBuffer) : nullptr;
        for (uint32_t i = 0; callData && i < call_count; i++)
        {
            uint32_t shaderIndex = sbt.CallableIndices[i];
            const uint8_t *dest = callData + (i * call_size);
            get_handles_for_sbtbuffer(pipeline, shaderIndex, 1, (void *)dest);
        }

        // unmap all the buffers
        if (rgenData)
            UnmapBuffer(buffer.RayGenBuffer);
        if (missData)
            UnmapBuffer(buffer.MissBuffer);
        if (hitData)
            UnmapBuffer(buffer.HitGroupBuffer);
        if (callData)
            UnmapBuffer(buffer.CallableBuffer);

        // Some groups may have gotten additional shaders, so we need to update the stride and size of the regions
        // We don't have to worry about the buffer sizes if they don't fit as it is already checked at the beginning of
        // this function

        if (rgen_count > 0)
            buffer.RayGenRegion = vk::StridedDeviceAddressRegionKHR()
                                      .setDeviceAddress(buffer.RayGenBuffer.DevAddress)
                                      .setStride(rgen_size)
                                      .setSize(rgen_size * rgen_count);
        if (miss_count > 0)
            buffer.MissRegion = vk::StridedDeviceAddressRegionKHR()
                                    .setDeviceAddress(buffer.MissBuffer.DevAddress)
                                    .setStride(miss_size)
                                    .setSize(miss_size * miss_count);
        if (hit_count > 0)
            buffer.HitGroupRegion = vk::StridedDeviceAddressRegionKHR()
                                        .setDeviceAddress(buffer.HitGroupBuffer.DevAddress)
                                        .setStride(hit_size)
                                        .setSize(hit_size * hit_count);
        if (call_count > 0)
            buffer.CallableRegion = vk::StridedDeviceAddressRegionKHR()
                                        .setDeviceAddress(buffer.CallableBuffer.DevAddress)
                                        .setStride(call_size)
                                        .setSize(call_size * call_count);

        return true;
    }

    void vk_ray_device::CopySBT(SBTBuffer &src, SBTBuffer &dst)
    {
        uint8_t *dstRgenData = dst.RayGenRegion.size > 0 ? (uint8_t *)MapBuffer(dst.RayGenBuffer) : nullptr;
        uint8_t *dstMissData = dst.MissRegion.size > 0 ? (uint8_t *)MapBuffer(dst.MissBuffer) : nullptr;
        uint8_t *dstHitData = dst.HitGroupRegion.size > 0 ? (uint8_t *)MapBuffer(dst.HitGroupBuffer) : nullptr;
        uint8_t *dstCallData = dst.CallableRegion.size > 0 ? (uint8_t *)MapBuffer(dst.CallableBuffer) : nullptr;

        uint8_t *srcRgenData = src.RayGenRegion.size > 0 ? (uint8_t *)MapBuffer(src.RayGenBuffer) : nullptr;
        uint8_t *srcMissData = src.MissRegion.size > 0 ? (uint8_t *)MapBuffer(src.MissBuffer) : nullptr;
        uint8_t *srcHitData = src.HitGroupRegion.size > 0 ? (uint8_t *)MapBuffer(src.HitGroupBuffer) : nullptr;
        uint8_t *srcCallData = src.CallableRegion.size > 0 ? (uint8_t *)MapBuffer(src.CallableBuffer) : nullptr;

        if (dstRgenData && srcRgenData)
            memcpy(dstRgenData, srcRgenData, src.RayGenRegion.size);
        if (dstMissData && srcMissData)
            memcpy(dstMissData, srcMissData, src.MissRegion.size);
        if (dstHitData && srcHitData)
            memcpy(dstHitData, srcHitData, src.HitGroupRegion.size);
        if (dstCallData && srcCallData)
            memcpy(dstCallData, srcCallData, src.CallableRegion.size);

        if (dstRgenData)
            UnmapBuffer(dst.RayGenBuffer);
        if (dstMissData)
            UnmapBuffer(dst.MissBuffer);
        if (dstHitData)
            UnmapBuffer(dst.HitGroupBuffer);
        if (dstCallData)
            UnmapBuffer(dst.CallableBuffer);

        if (srcRgenData)
            UnmapBuffer(src.RayGenBuffer);
        if (srcMissData)
            UnmapBuffer(src.MissBuffer);
        if (srcHitData)
            UnmapBuffer(src.HitGroupBuffer);
        if (srcCallData)
            UnmapBuffer(src.CallableBuffer);
    }


    bool vk_ray_device::CanSBTFitShaders(SBTBuffer &buffer, const SBTInfo &sbtInfo) {

        bool extendable = true;

        const uint32_t rgen_size = AlignUp(sbtInfo.RayGenShaderRecordSize + m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t miss_size = AlignUp(sbtInfo.MissShaderRecordSize + m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t hit_size = AlignUp(sbtInfo.HitGroupRecordSize + m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);
        const uint32_t call_size = AlignUp(sbtInfo.CallableShaderRecordSize + m_ray_tracing_properties.shaderGroupHandleSize, m_ray_tracing_properties.shaderGroupHandleAlignment);

        const uint32_t rgen_bytes_needed = sbtInfo.RayGenIndices.size() * rgen_size;
        const uint32_t miss_bytes_needed = sbtInfo.MissIndices.size() * miss_size;
        const uint32_t hit_bytes_needed = sbtInfo.HitGroupIndices.size() * hit_size;
        const uint32_t call_bytes_needed = sbtInfo.CallableIndices.size() * call_size;

        if (rgen_bytes_needed > buffer.RayGenBuffer.Size)       return false;
        if (miss_bytes_needed > buffer.MissBuffer.Size)         return false;
        if (hit_bytes_needed > buffer.HitGroupBuffer.Size)      return false;
        if (call_bytes_needed > buffer.CallableBuffer.Size)     return false;

        return extendable;
    }


    void vk_ray_device::DestroySBTBuffer(SBTBuffer &buffer){

        // destroy all the buffers if they were created
        if (buffer.RayGenBuffer.Buffer)
            DestroyBuffer(buffer.RayGenBuffer);
        if (buffer.MissBuffer.Buffer)
            DestroyBuffer(buffer.MissBuffer);
        if (buffer.HitGroupBuffer.Buffer)
            DestroyBuffer(buffer.HitGroupBuffer);
        if (buffer.CallableBuffer.Buffer)
            DestroyBuffer(buffer.CallableBuffer);
        buffer.RayGenRegion = vk::StridedDeviceAddressRegionKHR();
        buffer.MissRegion = vk::StridedDeviceAddressRegionKHR();
        buffer.HitGroupRegion = vk::StridedDeviceAddressRegionKHR();
        buffer.CallableRegion = vk::StridedDeviceAddressRegionKHR();
    }

    // CLASS PROTECTED =================================================================================================

    // CLASS PRIVATE ===================================================================================================

}
