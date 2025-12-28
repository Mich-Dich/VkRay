#pragma once

#include <iostream>
#include <memory>
#include <numeric>
#include <set>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "VkRay/AccelStruct.h"
#include "VkRay/Buffer.h"
#include "VkRay/Descriptors.h"
#include "VkRay/SBT.h"
#include "VkRay/Shader.h"
#include "VkRay/VkRay_device.h"

#define VULRAY_LOG_STREAM std::cerr
#define VULRAY_LOG_END "\033[0m"

namespace vr
{

    // VkRay Log Callback
    typedef void (*VkRayLogCallback)(const std::string &, MessageType);

    // Logging Callback variable for vulray, if set, vulray will call this function instead of printing to console
    // if specified, Vulkan validation layer messages will be relayed to this callback if a
    // VulkanBuilder::DebugCallback is not specified when building the vulkan instance using VkRay::VulkanBuilder
    extern VkRayLogCallback LogCallback;

    // Stackoverflow: https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    // Guard against multiple definitions

    namespace detail
    {
        extern char _gStringFormatBuffer[1024];
    }
#ifndef _VULRAY_LOG_DEF
#define _VULRAY_LOG_DEF
    template <typename... Args>
    constexpr std::string _string_format(const std::string &format, Args... args)
    {
        int size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
        if (size <= 0)
            return "LOGGING ERROR";
        std::snprintf(detail::_gStringFormatBuffer, size, format.c_str(), args...);
        return std::string(detail::_gStringFormatBuffer,
                           detail::_gStringFormatBuffer + size - 1); // We don't want the '\0' inside
    }
#endif
} // namespace vr

// FLOG means formatted log
#define VULRAY_FLOG_VERBOSE(fmt, ...)                                                                                \
    if (vr::LogCallback == nullptr)                                                                                  \
    {                                                                                                                \
        VULRAY_LOG_STREAM << "\x1B[90m [VkRay]: " << vr::_string_format(fmt, __VA_ARGS__) << VULRAY_LOG_END << "\n"; \
    }                                                                                                                \
    else                                                                                                             \
    {                                                                                                                \
        vr::LogCallback(vr::_string_format(fmt, __VA_ARGS__), vr::MessageType::Verbose);                             \
    }
#define VULRAY_FLOG_INFO(fmt, ...)                                                                                   \
    if (vr::LogCallback == nullptr)                                                                                  \
    {                                                                                                                \
        VULRAY_LOG_STREAM << "\033[0m  [VkRay]: " << vr::_string_format(fmt, __VA_ARGS__) << VULRAY_LOG_END << "\n"; \
    }                                                                                                                \
    else                                                                                                             \
    {                                                                                                                \
        vr::LogCallback(vr::_string_format(fmt, __VA_ARGS__), vr::MessageType::Info);                                \
    }
#define VULRAY_FLOG_WARNING(fmt, ...)                                                                                \
    if (vr::LogCallback == nullptr)                                                                                  \
    {                                                                                                                \
        VULRAY_LOG_STREAM << "\x1B[33m [VkRay]: " << vr::_string_format(fmt, __VA_ARGS__) << VULRAY_LOG_END << "\n"; \
    }                                                                                                                \
    else                                                                                                             \
    {                                                                                                                \
        vr::LogCallback(vr::_string_format(fmt, __VA_ARGS__), vr::MessageType::Warning);                             \
    }
#define VULRAY_FLOG_ERROR(fmt, ...)                                                                                  \
    if (vr::LogCallback == nullptr)                                                                                  \
    {                                                                                                                \
        VULRAY_LOG_STREAM << "\x1B[31m [VkRay]: " << vr::_string_format(fmt, __VA_ARGS__) << VULRAY_LOG_END << "\n"; \
    }                                                                                                                \
    else                                                                                                             \
    {                                                                                                                \
        vr::LogCallback(vr::_string_format(fmt, __VA_ARGS__), vr::MessageType::Error);                               \
    }


#define VULRAY_LOG_VERBOSE(message)                                                     \
    if (vr::LogCallback == nullptr)                                                     \
    {                                                                                   \
        VULRAY_LOG_STREAM << "\x1B[90m [VkRay]: " << message << VULRAY_LOG_END << "\n"; \
    }                                                                                   \
    else                                                                                \
    {                                                                                   \
        vr::LogCallback(message, vr::MessageType::Verbose);                             \
    }
#define VULRAY_LOG_INFO(message)                                                        \
    if (vr::LogCallback == nullptr)                                                     \
    {                                                                                   \
        VULRAY_LOG_STREAM << "\033[0m  [VkRay]: " << message << VULRAY_LOG_END << "\n"; \
    }                                                                                   \
    else                                                                                \
    {                                                                                   \
        vr::LogCallback(message, vr::MessageType::Info);                                \
    }
#define VULRAY_LOG_WARNING(message)                                                     \
    if (vr::LogCallback == nullptr)                                                     \
    {                                                                                   \
        VULRAY_LOG_STREAM << "\x1B[33m [VkRay]: " << message << VULRAY_LOG_END << "\n"; \
    }                                                                                   \
    else                                                                                \
    {                                                                                   \
        vr::LogCallback(message, vr::MessageType::Warning);                             \
    }
#define VULRAY_LOG_ERROR(message)                                                       \
    if (vr::LogCallback == nullptr)                                                     \
    {                                                                                   \
        VULRAY_LOG_STREAM << "\x1B[31m [VkRay]: " << message << VULRAY_LOG_END << "\n"; \
    }                                                                                   \
    else                                                                                \
    {                                                                                   \
        vr::LogCallback(message, vr::MessageType::Error);                               \
    }
