#pragma once

#include "../../src/pch.h"

// FORWARD DECLARATIONS ================================================================================================

namespace vr {

    // CONSTANTS =======================================================================================================

    // MACROS ==========================================================================================================

    // TYPES ===========================================================================================================

    // @brief Structure of a Buffer used for calls with VkRay
    struct allocated_buffer
    {
        // @brief The allocation for the buffer, if this is null, the buffer is not allocated by VkRay
        // @note If this is not null, the buffer is allocated by VkRay and should be freed by calling
        // DestroyBuffer(...) If the buffer is allocated by the user, they do not need to set this field.
        VmaAllocation           Allocation = nullptr;
        vk::Buffer              Buffer = nullptr;                       // @brief The raw buffer handle
        vk::DeviceAddress       DevAddress = 0;                         // @brief The device address of the buffer
        uint64_t                Size = 0;                               // @brief The size of the buffer, without any alignment
    };

    struct AllocatedTexelBuffer {

        allocated_buffer        Buffer;                                 // @brief The allocation for the buffer
        vk::Format              Format = vk::Format::eUndefined;        // @brief The format of the texel buffer
    };

    // @brief Structure of an Image, to use with VkRay
    struct AllocatedImage {

        // @brief The allocation for the image, if this is null, the image is not allocated by VkRay
        // @note If this is not null, the image is allocated by VkRay and should be freed by calling DestroyImage(...)
        // If the buffer is allocated by the user, they do not need to set this field.
        VmaAllocation           Allocation = nullptr;
        vk::Image               Image = nullptr;                        // @brief The raw image handle
        uint32_t                Width = 0;
        uint32_t                Height = 0;
        uint64_t                Size = 0;
    };

    // @brief Structure of an Image, to use with VkRay
    // @note Primarily used for images for DescriptorSets
    struct AccessibleImage {

        vk::ImageView           View = nullptr;                         // @brief The image view of the image
        vk::Sampler             Sampler = nullptr;                      // @brief Optional sampler for the image
        vk::ImageLayout         Layout = vk::ImageLayout::eUndefined;   // @brief The image layout of the image
    };

    // STATIC VARIABLES ================================================================================================

    // FUNCTION DECLARATION ============================================================================================

    // @brief Aligns a value up to the specified alignment
    // @param value The value to align
    // @param alignment The alignment to align the value to
    // @return The aligned value
    uint32_t AlignUp(uint32_t value, uint32_t alignment);

    // @brief Aligns a value up to the specified alignment
    // @param value The value to align
    // @param alignment The alignment to align the value to
    // @return The aligned value
    uint64_t AlignUp(uint64_t value, uint64_t alignment);

    // TEMPLATE DECLARATION ============================================================================================

    // CLASS DECLARATION ===============================================================================================

}
