
#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

// FORWARD DECLARATIONS ================================================================================================


namespace vr {

    // CONSTANTS =======================================================================================================

    // MACROS ==========================================================================================================

    // TYPES ===========================================================================================================

    struct InstanceWrapper
    {
        vk::Instance                                    InstanceHandle;
        vk::DebugUtilsMessengerEXT                      DebugMessenger;

        static void DestroyInstance(vk::Instance instance, vk::DebugUtilsMessengerEXT messenger = nullptr);

        static void DestroyInstance(InstanceWrapper instance);
    };

    struct swapchain_resources
    {

        vk::SwapchainKHR                                SwapchainHandle = nullptr;
        std::vector<vk::Image>                          SwapchainImages;
        std::vector<vk::ImageView>                      SwapchainImageViews;
        vk::Format                                      SwapchainFormat;
        vk::Extent2D                                    SwapchainExtent;
    };

    struct CommandQueues
    {
        vk::Queue                                       GraphicsQueue = nullptr;
        vk::Queue                                       ComputeQueue = nullptr;
        vk::Queue                                       TransferQueue = nullptr;
        vk::Queue                                       PresentQueue = nullptr;

        uint32_t                                        GraphicsIndex = ~0U;
        uint32_t                                        ComputeIndex = ~0U;
        uint32_t                                        TransferIndex = ~0U;
        uint32_t                                        PresentIndex = ~0U;
    };

    // STATIC VARIABLES ================================================================================================

    // FUNCTION DECLARATION ============================================================================================

    std::vector<const char *> GetRequiredExtensionsForVkRay();

    // TEMPLATE DECLARATION ============================================================================================

    // CLASS DECLARATION ===============================================================================================

    // Convenience wrapper for building Vulkan devices quickly wihtout own setup
    // All the Vulkan structures that are built MUST use the same vulkan_builder, because it keeps track of the internal
    // state Client has to destroy the created objects
    struct vulkan_builder
    {
        // Fully transparent class

        vulkan_builder();
        ~vulkan_builder();

        [[nodiscard]] InstanceWrapper CreateInstance();

        [[nodiscard]] vk::PhysicalDevice PickPhysicalDevice(vk::SurfaceKHR surface);

        [[nodiscard]] vk::Device CreateDevice();

        // Call after CreateDevice() to get all the needed queues the device
        [[nodiscard]] CommandQueues GetQueues();

        bool                                            EnableDebug = false;                    // Enables validation layers
        std::vector<vk::ValidationFeatureEnableEXT>     ValidationFeatures;                     // Enables raytracing extensions
        bool                                            DedicatedCompute = false;               // Device creation will fail if the device does not support the needed dedicated queues
        bool                                            DedicatedTransfer = false;
        VkPhysicalDeviceFeatures                        PhysicalDeviceFeatures10 = {};
        VkPhysicalDeviceVulkan11Features                PhysicalDeviceFeatures11 = {};
        VkPhysicalDeviceVulkan12Features                PhysicalDeviceFeatures12 = {};
        VkPhysicalDeviceVulkan13Features                PhysicalDeviceFeatures13 = {};

        std::vector<const char*>                        InstanceExtensions;                     // Debug layers are added automatically if EnableDebug is true
        std::vector<const char*>                        InstanceLayers;
        std::vector<const char*>                        DeviceExtensions;                       // Raytracing extensions are added automatically, no need to add them
        PFN_vkDebugUtilsMessengerCallbackEXT            DebugCallback = nullptr;                // set nullptr to use default VkRay callback
        void*                                           DebugCallbackUserData = nullptr;        // The pointer to the debug callback user data

    private:

        std::shared_ptr<void>                           struct_data = nullptr;

    };

    // utility to create swapchain, all resources that are created must be destroyed by the client
    struct swapchain_builder
    {
        swapchain_builder() = default;
        swapchain_builder(vk::Device dev, vk::PhysicalDevice physdev, vk::SurfaceKHR surf, uint32_t gfxQueueidx, uint32_t presentQueueidx);

        // Structures need to be filled before building
        vk::Device                                      Device = nullptr;
        vk::PhysicalDevice                              PhysicalDevice = nullptr;
        vk::SurfaceKHR                                  Surface = nullptr;
        uint32_t                                        GraphicsQueueIndex = UINT32_MAX;
        uint32_t                                        PresentQueueIndex = UINT32_MAX;
        uint32_t                                        Height = 1, Width = 1;
        uint32_t                                        BackBufferCount = 2;
        vk::ImageUsageFlags                             ImageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        vk::Format                                      DesiredFormat = vk::Format::eB8G8R8A8Srgb;
        vk::ColorSpaceKHR                               ColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        vk::PresentModeKHR                              PresentMode = vk::PresentModeKHR::eMailbox;

        // Creates a swapchain from the settings
        // if oldswapchain is passed it uses that swapchain as an oldswapchain to recreate the swapchain.
        // not using oldswapchain works fine too
        // The previous swapchain will not be destroyed, the client has to destroy it via DestroySwapchain()
        [[nodiscard]] swapchain_resources BuildSwapchain(vk::SwapchainKHR oldswapchain = nullptr);

        // This destroys the swapchain supplies
        static void DestroySwapchain(vk::Device device, const swapchain_resources &res);
        // This destroys the swapchain resources, but not the swapchain itself, useful for recreating the swapchain,
        // because the oldswapchain is needed The client has to destroy the swapchain handle
        static void DestroySwapchainResources(vk::Device device, const swapchain_resources &res);

    private:

        std::shared_ptr<void>                           struct_data = nullptr;                  // pointer to vkb::Swapchain

    };

}
