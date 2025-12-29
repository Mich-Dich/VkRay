#include "VkRay/Shader.h"

namespace vr {

    Shader VkRayDevice::CreateShaderFromSPV(const std::vector<uint32_t> &spv) {

        Shader outShader = {};
        if (spv.empty()) {

            VULRAY_LOG_ERROR("ShaderCreateInfo must have SPIRV code");
            return outShader; // return empty shader, because no shader was created
        }

        outShader.Module = CreateShaderModule(spv);
        return outShader;
    }


    void VkRayDevice::DestroyShader(Shader &shader)         { mDevice.destroyShaderModule(shader.Module); }


    vk::ShaderModule VkRayDevice::CreateShaderModule(const std::vector<uint32_t> &spvCode) {

        // create shader module
        auto shaderModuleCreateInfo = vk::ShaderModuleCreateInfo().setCodeSize(spvCode.size() * sizeof(uint32_t)).setPCode(spvCode.data());
        auto shaderModule = mDevice.createShaderModule(shaderModuleCreateInfo);
        return shaderModule;
    }

}
