
#include "pch.h"

#include "VkRay/VkRay_device.h"
#include "VkRay/Shader.h"

// FORWARD DECLARATIONS ================================================================================================

namespace vr {

    // CONSTANTS =======================================================================================================

    // MACROS ==========================================================================================================

    // TYPES ===========================================================================================================

    // STATIC VARIABLES ================================================================================================

    // FUNCTION IMPLEMENTATION =========================================================================================

    // CLASS IMPLEMENTATION ============================================================================================

    Shader vk_ray_device::CreateShaderFromSPV(const std::vector<uint32_t>& spv) {

        Shader outShader = {};
        if (spv.empty()) {

            VR_LOG(error, "ShaderCreateInfo must have SPIRV code");
            return outShader; // return empty shader, because no shader was created
        }

        outShader.Module = CreateShaderModule(spv);
        return outShader;
    }


    void vk_ray_device::DestroyShader(Shader &shader) { m_device.destroyShaderModule(shader.Module); }


    vk::ShaderModule vk_ray_device::CreateShaderModule(const std::vector<uint32_t> &spvCode) {

        // create shader module
        auto shaderModuleCreateInfo = vk::ShaderModuleCreateInfo().setCodeSize(spvCode.size() * sizeof(uint32_t)).setPCode(spvCode.data());
        auto shaderModule = m_device.createShaderModule(shaderModuleCreateInfo);
        return shaderModule;
    }

    // CLASS PUBLIC ====================================================================================================

    // CLASS PROTECTED =================================================================================================

    // CLASS PRIVATE ===================================================================================================

}
