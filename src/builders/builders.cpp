
#include "../pch.h"

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include "VkRay/builders/builders.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL VkRayVulkanDebugCback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

namespace vr
{

    // CONSTANTS =======================================================================================================

    static std::vector<const char *> RayTracingExtensions = {
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // required by accel struct extension
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,     // Required by VkRay if using Descriptors that VkRay creates
        VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, // for independent sets
        VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME // for ray tracing position fet
    };

    // MACROS ==========================================================================================================

    // TYPES ===========================================================================================================

    struct builder_vkb_structs
    {
        vkb::Instance Instance;
        vkb::PhysicalDevice PhysicalDevice;
        vkb::Device Device;
    };

    // STATIC VARIABLES ================================================================================================

    // FUNCTION IMPLEMENTATION =========================================================================================

    void InstanceWrapper::DestroyInstance(vk::Instance instance, vk::DebugUtilsMessengerEXT messenger)
    {

        if (instance)
        {
            if (messenger)
            {

                auto vkDestroyDebugUtilsMessengerEXT_ = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                vkDestroyDebugUtilsMessengerEXT_(instance, messenger, nullptr);
            }
            instance.destroy();
            return;
        }
        VR_LOG(warning, "Called Instance::DestroyInstance with invalid vk::Instance");
    }

    void InstanceWrapper::DestroyInstance(InstanceWrapper instance) { InstanceWrapper::DestroyInstance(instance.InstanceHandle, instance.DebugMessenger); }

    std::vector<const char *> GetRequiredExtensionsForVkRay() { return RayTracingExtensions; }

    // CLASS IMPLEMENTATION ============================================================================================

    vulkan_builder::vulkan_builder()
    {

        struct_data = std::make_shared<builder_vkb_structs>();
    }

    vulkan_builder::~vulkan_builder() {}

    // CLASS PUBLIC ====================================================================================================

    InstanceWrapper vulkan_builder::CreateInstance()
    {

        auto inst_builder = vkb::InstanceBuilder().require_api_version(1, 3, 0);

        for (auto v : ValidationFeatures)
            inst_builder.add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(v));

        for (auto &ext : InstanceExtensions)
            inst_builder.enable_extension(ext);

        for (auto &layer : InstanceLayers)
            inst_builder.enable_layer(layer);

        // required extensions by VkRay
        inst_builder.enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        if (EnableDebug)
        {
            inst_builder.request_validation_layers()
                .set_debug_callback_user_data_pointer(DebugCallbackUserData)
                .set_debug_callback(DebugCallback == nullptr ? VkRayVulkanDebugCback : DebugCallback)
                .set_debug_messenger_severity(
                    (VkDebugUtilsMessageSeverityFlagsEXT)(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose))
                .add_debug_messenger_type(
                    (VkDebugUtilsMessageTypeFlagsEXT)(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance));
        }

        auto instance_result = inst_builder.build();
        if (!instance_result.has_value())
        {

            VR_LOG(error, "Instance Build failed, Error: %s", instance_result.error().message());
            throw std::runtime_error("No Instance Created");
            return {};
        }

        vkb::Instance &return_structs = reinterpret_cast<builder_vkb_structs *>(struct_data.get())->Instance;
        return_structs = instance_result.value();

        return {return_structs.instance, return_structs.debug_messenger};
    }

    vk::PhysicalDevice vulkan_builder::PickPhysicalDevice(vk::SurfaceKHR surface)
    {

        vkb::Instance &instance = reinterpret_cast<builder_vkb_structs *>(struct_data.get())->Instance;

        auto phys_selector = vkb::PhysicalDeviceSelector(instance, surface)
                                 .add_required_extensions(DeviceExtensions)
                                 .add_required_extensions(RayTracingExtensions)
                                 .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                 .set_surface(surface)
                                 .require_present();

        // Enable needed features
        auto raytracing_features = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR().setRayTracingPipeline(true);
        auto ray_query_features = vk::PhysicalDeviceRayQueryFeaturesKHR().setRayQuery(true);
        auto ray_tracing_position_fetch_features = vk::PhysicalDeviceRayTracingPositionFetchFeaturesKHR().setRayTracingPositionFetch(true);

        auto accelFeatures = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
                                 .setAccelerationStructure(true)
                                 .setDescriptorBindingAccelerationStructureUpdateAfterBind(true);

        auto descbufferFeatures = vk::PhysicalDeviceDescriptorBufferFeaturesEXT()
                                      .setDescriptorBuffer(true)
                                      .setDescriptorBufferImageLayoutIgnored(true);

        phys_selector.add_required_extension_features(raytracing_features);
        phys_selector.add_required_extension_features(ray_query_features);
        phys_selector.add_required_extension_features(ray_tracing_position_fetch_features);
        phys_selector.add_required_extension_features(accelFeatures);
        phys_selector.add_required_extension_features(descbufferFeatures);

        PhysicalDeviceFeatures12.bufferDeviceAddress = true;
        PhysicalDeviceFeatures12.descriptorIndexing = true;
        PhysicalDeviceFeatures12.descriptorBindingVariableDescriptorCount = true;
        PhysicalDeviceFeatures12.descriptorBindingPartiallyBound = true;
        PhysicalDeviceFeatures12.runtimeDescriptorArray = true;
        PhysicalDeviceFeatures12.shaderSampledImageArrayNonUniformIndexing = true;
        PhysicalDeviceFeatures12.shaderStorageBufferArrayNonUniformIndexing = true;
        PhysicalDeviceFeatures12.shaderStorageImageArrayNonUniformIndexing = true;
        PhysicalDeviceFeatures12.shaderUniformBufferArrayNonUniformIndexing = true;
        PhysicalDeviceFeatures12.shaderUniformTexelBufferArrayNonUniformIndexing = true;
        PhysicalDeviceFeatures12.shaderStorageTexelBufferArrayNonUniformIndexing = true;

        phys_selector.set_required_features(PhysicalDeviceFeatures10);
        phys_selector.set_required_features_11(PhysicalDeviceFeatures11);
        phys_selector.set_required_features_12(PhysicalDeviceFeatures12);
        phys_selector.set_required_features_13(PhysicalDeviceFeatures13);

        phys_selector.set_minimum_version(1, 2);
        auto phys_result = phys_selector.select();
        if (!phys_result.has_value())
        {
            VR_LOG(error, "Physical Device Build failed, Error: %s", phys_result.error().message());
            throw std::runtime_error("Physical Device Build failed");
            return {};
        }

        vkb::PhysicalDevice &return_struct = reinterpret_cast<builder_vkb_structs *>(struct_data.get())->PhysicalDevice;
        return_struct = phys_result.value();

        return return_struct.physical_device;
    }

    vk::Device vulkan_builder::CreateDevice()
    {

        vkb::PhysicalDevice &phys_dev = reinterpret_cast<builder_vkb_structs *>(struct_data.get())->PhysicalDevice;
        auto dev_builder = vkb::DeviceBuilder(phys_dev);
        auto devResult = dev_builder.build();

        if (!devResult.has_value())
        {

            VR_LOG(error, "Logical Device Build failed, Error: %s", devResult.error().message());
            throw std::runtime_error("No Logical Devices Created");
            return {};
        }
        vkb::Device &dev = reinterpret_cast<builder_vkb_structs *>(struct_data.get())->Device;
        dev = devResult.value();
        return dev.device;
    }

    CommandQueues vulkan_builder::GetQueues()
    {

        vkb::Device &device = reinterpret_cast<builder_vkb_structs *>(struct_data.get())->Device;
        auto vk_device = static_cast<vk::Device>(device.device);
        CommandQueues queues = {};

        auto getQueue = [&](uint32_t index) -> vk::Queue
        {
            return vk_device.getQueue(index, 0);
        };

        bool dedCompute = false;
        bool dedTransfer = false;

        for (uint32_t x = 0; x < device.queue_families.size(); x++)
        {

            auto &queue = device.queue_families[x];
            auto currentQueue = getQueue(x);

            if (static_cast<vk::QueueFlags>(queue.queueFlags) & vk::QueueFlagBits::eGraphics)
            {

                queues.GraphicsQueue = currentQueue;
                queues.GraphicsIndex = x;

                queues.PresentQueue = currentQueue;
                queues.PresentIndex = x;
            }

            if (static_cast<vk::QueueFlags>(queue.queueFlags) & vk::QueueFlagBits::eCompute)
            {

                if (dedCompute)
                    continue; // If we already have a dedicated compute queue, skip

                if (queues.GraphicsIndex == x)
                    dedCompute = false;
                else
                    dedCompute = true;

                queues.ComputeQueue = currentQueue;
                queues.ComputeIndex = x;
            }

            if (static_cast<vk::QueueFlags>(queue.queueFlags) & vk::QueueFlagBits::eTransfer)
            {

                if (dedTransfer)
                    continue;

                if (queues.GraphicsIndex == x || queues.ComputeIndex == x)
                    dedTransfer = false;
                else
                    dedTransfer = true;

                queues.TransferQueue = currentQueue;
                queues.TransferIndex = x;
            }
        }

        if (!queues.GraphicsQueue)
        {

            VR_LOG(error, "No Graphics Queue Found");
            throw std::runtime_error("No Graphics Queue Found");
        }

        if (DedicatedCompute && !dedCompute)
        {

            VR_LOG(error, "No Compute Queue Found");
            throw std::runtime_error("No Compute Queue Found");
        }

        if (DedicatedTransfer && !dedTransfer)
        {

            VR_LOG(error, "No Transfer Queue Found");
            throw std::runtime_error("No Transfer Queue Found");
        }

        return queues;
    }

    // CLASS PROTECTED =================================================================================================

    // CLASS PRIVATE ===================================================================================================

    // CLASS IMPLEMENTATION ============================================================================================

    swapchain_builder::swapchain_builder(vk::Device dev, vk::PhysicalDevice physdev, vk::SurfaceKHR surf, uint32_t gfxQueueidx, uint32_t presentQueueidx)
        : Device(dev), PhysicalDevice(physdev), Surface(surf), GraphicsQueueIndex(gfxQueueidx), PresentQueueIndex(presentQueueidx) {}

    // CLASS PUBLIC ====================================================================================================

    swapchain_resources swapchain_builder::BuildSwapchain(vk::SwapchainKHR oldswapchain)
    {

        assert(Height != 0);
        assert(Width != 0);
        assert(Device);
        assert(PhysicalDevice);
        assert(GraphicsQueueIndex != UINT32_MAX);
        assert(PresentQueueIndex != UINT32_MAX);
        assert(PhysicalDevice);
        assert(Surface);

        std::vector<vk::SurfaceFormatKHR> surface_formats = PhysicalDevice.getSurfaceFormatsKHR(Surface);
        vk::SurfaceFormatKHR compatible_format = {};
        bool found_surface_format = false;
        // See if a compatible format is found, we don't guarantee the colorspace
        // will be the same, but we try to find the desired format
        for (auto format : surface_formats)
        {

            // if it fulfills our criteria
            if (format.format == DesiredFormat && format.colorSpace == ColorSpace)
            {

                compatible_format = format;
                found_surface_format = true;
                break;
            }

            if (format.format == DesiredFormat)
            {

                compatible_format = format;
                found_surface_format = true;
            }
        }

        if (compatible_format.format != DesiredFormat)
            VR_LOG(error, "Desired Format is not available, Fallback is vk::Format::eB8G8R8A8Srgb");

        if (compatible_format.colorSpace != ColorSpace)
            VR_LOG(error, "Desired ColorSpace is not available, Using: %s", vk::to_string(compatible_format.colorSpace));

        vkb::SwapchainBuilder *swap_builder = nullptr;
        swap_builder = new vkb::SwapchainBuilder(PhysicalDevice, Device, Surface, GraphicsQueueIndex, PresentQueueIndex);

        if (oldswapchain)
            swap_builder->set_old_swapchain(oldswapchain);

        if (found_surface_format)
            swap_builder->set_desired_format(compatible_format);

        else
            swap_builder->use_default_format_selection();

        swap_builder->set_desired_extent(Width, Height)
            .set_desired_present_mode((VkPresentModeKHR)PresentMode)
            .add_fallback_present_mode((VkPresentModeKHR)vk::PresentModeKHR::eMailbox)
            .set_desired_min_image_count(BackBufferCount) /*copy raytracing texture*/
            .set_image_usage_flags((VkImageUsageFlags)(ImageUsage | vk::ImageUsageFlagBits::eColorAttachment));

        auto build_result = swap_builder->build();
        if (!build_result.has_value())
        {

            VR_LOG(error, "Swapchain Build failed, Error: %s", build_result.error().message());
            throw std::runtime_error("Swapchain Build failed");
            return {};
        }

        auto swapchain = build_result.value();
        auto swap_image_views = swapchain.get_image_views();
        auto swap_images = swapchain.get_images();

        if (!swap_images.has_value())
            VR_LOG(error, "Swapchain Images Error: %s", swap_images.error().message());

        if (!swap_image_views.has_value())
            VR_LOG(error, "Swapchain Image Views Error: %s", swap_image_views.error().message());

        auto images = *reinterpret_cast<std::vector<vk::Image> *>(&swap_images.value());
        auto imageviews = *reinterpret_cast<std::vector<vk::ImageView> *>(&swap_image_views.value());
        return {swapchain.swapchain, images, imageviews, (vk::Format)swapchain.image_format, (vk::Extent2D)swapchain.extent};
    }

    void swapchain_builder::DestroySwapchain(vk::Device device, const swapchain_resources &resource)
    {

        device.destroySwapchainKHR(resource.SwapchainHandle);
        for (auto image_view : resource.SwapchainImageViews)
            device.destroyImageView(image_view);
    }

    void swapchain_builder::DestroySwapchainResources(vk::Device device, const swapchain_resources &resource)
    {

        for (auto image_view : resource.SwapchainImageViews)
            device.destroyImageView(image_view);
    }

    // CLASS PROTECTED =================================================================================================

    // CLASS PRIVATE ===================================================================================================

}


static VKAPI_ATTR VkBool32 VKAPI_CALL VkRayVulkanDebugCback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{

    std::string msgHead = "[Vulkan][";
    const char *msgSeverity = vkb::to_string_message_severity(messageSeverity);
    const char *msgType = vkb::to_string_message_type(messageType);
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        VR_LOG(verbose, "[Vulkan][%s][%s]: %s", msgType, msgSeverity, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        VR_LOG(info, "[Vulkan][%s][%s]: %s", msgType, msgSeverity, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        VR_LOG(warning, "[Vulkan][%s][%s]: %s", msgType, msgSeverity, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        VR_LOG(error, "[Vulkan][%s][%s]: %s", msgType, msgSeverity, pCallbackData->pMessage);
        break;
    default:
        break;
    }

    return VK_FALSE;
}
