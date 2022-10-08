#include "VulkanFactory.h"

VulkanFactory* VulkanFactory::m_Instance = nullptr;
std::mutex VulkanFactory::m_Mutex;

VulkanFactory::~VulkanFactory() {
    CleanupSwapChain();

    for (auto semaphore : m_ImageReadySemaphores) {
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    }
    for (auto semaphore : m_RenderFinishedSemaphores) {
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    }

    vkDestroySurfaceKHR(m_VkInstance, m_Surface, nullptr);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroyInstance(m_VkInstance, nullptr);
}

void VulkanFactory::InitVulkan(GLFWwindow* window) {
    m_Window = window;
    CreateVkInstance();
    if (glfwCreateWindowSurface(m_VkInstance, m_Window, nullptr, &m_Surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
    vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR = reinterpret_cast<PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR>(vkGetInstanceProcAddr(m_VkInstance, "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR"));
    PickPhysicalDevice();
    CreateLogicalDevice();
    QueryFunctionPointers();
    CreateSwapChain();
    CreateSwapchainImageViews();
    CreateCommandPool();
    CreateDepthResources();
    CreateRenderPass();
    CreateFramebuffers();
    CreateSemaphores();
}

void VulkanFactory::CreateVkInstance() {
    VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,                         // sType
        nullptr,                                                    // pNext
        "master thesis engine",                                     // pApplicationName
        VK_MAKE_VERSION(1,0,0),                                     // applicationVersion
        "PiotrDulskiEngine",                                        // pEngineName
        VK_MAKE_VERSION(1,0,0),                                     // engineVersion
        VK_API_VERSION_1_3                                          // apiVersion
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    uint32_t extensionsCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);

    std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, extensionProperties.data());

    std::cout << "available extensions:\n";
    auto checkExtensionsAvailable = [](const char* extension, std::vector<VkExtensionProperties>& extensions) {
        for (const auto& ext : extensions) {
            if (!strcmp(ext.extensionName, extension)) {
                std::cout << "extension " << extension << " available\n";
                return true;
            }
        }
        std::cout << "extension " << extension << " not available\n";
        return false;
    };
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
        if (!checkExtensionsAvailable(glfwExtensions[i], extensionProperties)) {
            throw std::runtime_error("required extension missing");
        }
    }

    auto checkValidationLayersSupport = []() {
        if (validationLayers.size() == 0) return false;
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (auto const& layer : validationLayers) {
            bool found = false;
            for (auto const& al : availableLayers) {
                if (!strcmp(al.layerName, layer)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("requested layer not present");
            }
        }
        return true;
    };

    bool supported = checkValidationLayersSupport();

    VkInstanceCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,                     // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        &appInfo,                                                   // pApplicationInfo
        (supported) ? (uint32_t)validationLayers.size() : 0,        // enabledLayerCount
        (supported) ? validationLayers.data() : nullptr,            // ppEnabledLayerNames
        glfwExtensionCount,                                         // enabledExtensionCount
        glfwExtensions                                              // ppEnabledExtensionNames
    };

    if (vkCreateInstance(&createInfo, nullptr, &m_VkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance");
    }
}

SwapChainSupportDetails VulkanFactory::QuerySwapChainSupport(const VkPhysicalDevice& physicalDevice) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);

    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, details.formats.data());
    }

    uint32_t presentModes = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModes, nullptr);

    if (presentModes) {
        details.presentModes.resize(presentModes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModes, details.presentModes.data());
    }

    return details;
}

void VulkanFactory::CreateSwapChain() {
    auto chooseFormat = [](const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& avFormat : availableFormats) {
            if (avFormat.format == VK_FORMAT_B8G8R8A8_SRGB && avFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                return avFormat;
            }
        }
        throw std::runtime_error("requested format not available");
        return availableFormats[0];
    };

    auto chooseSwapPresentMode = [](const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& avMode : availablePresentModes) {
            if (avMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return avMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    };

    auto chooseExtent = [&](const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width == UINT32_MAX) {
            return capabilities.currentExtent;
        }

        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);
        VkExtent2D actualExtent = { (uint32_t)width, (uint32_t)height };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    };

    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseExtent(swapChainSupport.capabilities);

    uint32_t imageCount = (swapChainSupport.capabilities.minImageCount > 3) ? swapChainSupport.capabilities.minImageCount :
        (swapChainSupport.capabilities.maxImageCount < 3) ? swapChainSupport.capabilities.maxImageCount : 3;

    VkSwapchainCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,                // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        m_Surface,                                                  // surface
        imageCount,                                                 // minImageCount
        surfaceFormat.format,                                       // imageFormat
        surfaceFormat.colorSpace,                                   // imageColorSpace
        extent,                                                     // imageExtent
        1,                                                          // imageArrayLayers
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,                        // imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                                  // imageSharingMode
        1,                                                          // queueFamilyIndexCount
        &m_QueueFamilyIndice.value(),                               // pQueueFamilyIndices
        swapChainSupport.capabilities.currentTransform,             // preTransform
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,                          // compositeAlpha
        presentMode,                                                // presentMode
        VK_TRUE,                                                    // clipped
        VK_NULL_HANDLE                                              // oldSwapchain
    };

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
    m_SwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

    m_SwapChainExtent = extent;
    m_SwapChainImageFormat = surfaceFormat.format;
}

void VulkanFactory::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_VkInstance, &deviceCount, nullptr);

    if (!deviceCount) {
        throw std::runtime_error("No GPU supporting vulkan");
    }

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);

    vkEnumeratePhysicalDevices(m_VkInstance, &deviceCount, physicalDevices.data());

    auto findQueueFamilies = [&](VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        uint32_t i = 0;
        for (const auto& queue : queueFamilies) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT && queue.timestampValidBits > 0) {
                indices = i;
                break;
            }
            ++i;
        }

        return indices;
    };

    auto isDeviceSuitable = [&](const VkPhysicalDevice& physicalDevice) {
        QueueFamilyIndices indice = findQueueFamilies(physicalDevice);

        uint32_t extensionsCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, nullptr);

        std::vector<VkExtensionProperties> extProperties(extensionsCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, extProperties.data());

        VkPhysicalDevicePerformanceQueryFeaturesKHR perfFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR };
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
        VkPhysicalDeviceFeatures2 supportedFeatures2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,           // sType
            &perfFeatures,                                          // pNext
            supportedFeatures                                       // features
        };
        vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures2);

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& ext : extProperties) {
            requiredExtensions.erase(ext.extensionName);
        }

        bool swapchainGood = false;
        if (!requiredExtensions.empty()) {
            return false;
        } else {
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
            swapchainGood = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

        VkPhysicalDeviceProperties deviceProperties{};
        VkPhysicalDeviceProperties2 deviceProperties2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,        // sType
            &rtProperties,                                         // pNext
            deviceProperties                                       // properties
        };
        vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

        if (rtProperties.maxRayRecursionDepth < 2) {
            return false;
        }


        if (indice.has_value()) {
            m_QueueFamilyIndice = indice;
            /*uint32_t counterCount = 0;
            vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, m_QueueFamilyIndice.value(), &counterCount, nullptr, nullptr);
            std::vector<VkPerformanceCounterKHR> counter(counterCount);
            std::vector<VkPerformanceCounterDescriptionKHR> counterDesc(counterCount);
            vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, m_QueueFamilyIndice.value(), &counterCount, counter.data(), counterDesc.data());*/
            return requiredExtensions.empty() && swapchainGood &&
                supportedFeatures.samplerAnisotropy;
        }
        return false;
    };

    for (const auto& device : physicalDevices) {
        if (isDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable GPU");
    }
}

void VulkanFactory::CreateLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,                 // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        m_QueueFamilyIndice.value(),                                // queueFamilyIndex
        1,                                                          // queueCount
        &queuePriority                                              // pQueuePriorities
    };

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vk12features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    vk12features.bufferDeviceAddress = VK_TRUE;
    vk12features.hostQueryReset = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeature{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    asFeature.accelerationStructure = VK_TRUE;

    vk12features.pNext = &rtPipelineFeatures;
    rtPipelineFeatures.pNext = &asFeature;

    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                       // sType
        &vk12features,                                                    // pNext
        0,                                                          // flags
        1,                                                          // queueCreateInfoCount
        &deviceQueueCreateInfo,                                     // pQueueCreateInfos
        (uint32_t)validationLayers.size(),                          // enabledLayerCount
        validationLayers.data(),                                    // ppEnabledLayerNames
        (uint32_t)deviceExtensions.size(),                          // enabledExtensionCount
        deviceExtensions.data(),                                    // ppEnabledExtensionNames
        &deviceFeatures                                             // pEnabledFeatures
    };

    vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device);

    vkGetDeviceQueue(m_Device, m_QueueFamilyIndice.value(), 0, &m_Queue);
}

void VulkanFactory::QueryFunctionPointers() {
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(m_Device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_Device, "vkCmdBuildAccelerationStructuresKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(m_Device, "vkCreateAccelerationStructureKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_Device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_Device, "vkCmdTraceRaysKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(m_Device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_Device, "vkCreateRayTracingPipelinesKHR"));
}

void VulkanFactory::RecreateSwapChain() {
    vkDeviceWaitIdle(m_Device);

    CleanupSwapChain();

    CreateSwapChain();
    CreateSwapchainImageViews();
    CreateRenderPass();
    CreateDepthResources();
    CreateFramebuffers();
    AllocateCommandBuffers();
}

void VulkanFactory::CleanupSwapChain() {
    vkDeviceWaitIdle(m_Device);
    vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
    vkDestroyImage(m_Device, m_DepthImage, nullptr);
    vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

    for (auto framebuffer : m_SwapChainFramebuffers) {
        vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
    }

    vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    for (auto imgView : m_SwapChainImageViews) {
        vkDestroyImageView(m_Device, imgView, nullptr);
    }
    vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

VkImageView VulkanFactory::CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask, VkImageViewType viewType, uint32_t facesCount, uint32_t mipLevels) {
    VkImageViewCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                   // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        img,                                                        // image
        viewType,                                                   // viewType
        format,                                                     // format
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },                                                          // components
        {
            aspectMask,                                         // aspectMask
            0,                                                  // baseMipLevel
            mipLevels,                                          // levelCount
            0,                                                  // baseArrayLayer
            facesCount,                                         // layerCount
        }                                                           // subresourceRange
    };
    VkImageView imgView;
    if (vkCreateImageView(m_Device, &createInfo, nullptr, &imgView) != VK_SUCCESS) {
        throw std::runtime_error("failed to created image view");
    }
    return imgView;
}

void VulkanFactory::CreateRenderPass() {
    VkAttachmentDescription colorAttachment = {
        0,                                                          // flags
        m_SwapChainImageFormat,                                     // format
        VK_SAMPLE_COUNT_1_BIT,                                      // samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,                                // loadOp
        VK_ATTACHMENT_STORE_OP_STORE,                               // storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                            // stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                           // stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,                                  // initialLayout
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR                             // finalLayout
    };

    VkAttachmentReference colorAttachmentReference = {
        0,                                                          // attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                    // layout
    };

    VkAttachmentDescription depthAttachment = {
        0,                                                          // flags
        VK_FORMAT_D24_UNORM_S8_UINT,                                // format
        VK_SAMPLE_COUNT_1_BIT,                                      // samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,                                // loadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                           // storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                            // stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                           // stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,                                  // initialLayout
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL            // finalLayout
    };

    VkAttachmentReference depthAttachmentReference = {
        1,                                                          // attachment
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL            // layout
    };

    VkSubpassDescription subpassDescription = {
        0,                                                          // flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                            // pipelineBindPoint
        0,                                                          // inputAttachmentCount
        nullptr,                                                    // pInputAttachments
        1,                                                          // colorAttachmentCount
        &colorAttachmentReference,                                  // pColorAttachments
        nullptr,                                                    // pResolveAttachments
        &depthAttachmentReference,                                 // pDepthStencilAttachment
        0,                                                          // preserveAttachmentCount
        nullptr                                                     // pPreserveAttachments
    };

    VkSubpassDependency subpassDependency = {
        VK_SUBPASS_EXTERNAL,                                        // srcSubpass
        0,                                                          // dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,             // srcStageMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,             // dstStageMask
        0,                                                          // srcAccessMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,           // dstAccessMask
        0                                                           // dependencyFlags
    };

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,                  // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        static_cast<uint32_t>(attachments.size()),                  // attachmentCount
        attachments.data(),                                         // pAttachments
        1,                                                          // subpassCount
        &subpassDescription,                                        // pSubpasses
        1,                                                          // dependencyCount
        &subpassDependency                                          // pDependencies
    };

    if (vkCreateRenderPass(m_Device, &createInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("cannot create render pass");
    }
}

void VulkanFactory::CreateSwapchainImageViews() {
    m_SwapChainImageViews.resize(m_SwapChainImages.size());
    for (size_t i = 0; i < m_SwapChainImages.size(); i++) {
        m_SwapChainImageViews[i] = CreateImageView(m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
    }
}

void VulkanFactory::CreateDepthResources(){
    CreateImage(m_SwapChainExtent.width, m_SwapChainExtent.height, 1, VK_FORMAT_D24_UNORM_S8_UINT,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory, 1);
    m_DepthImageView = CreateImageView(m_DepthImage, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
}

void VulkanFactory::CreateImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties, VkImage& img,
    VkDeviceMemory& imgMem, uint32_t arrayLayers, uint32_t flags, uint32_t mipLevels) {
    VkImageCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                        // sType
        nullptr,                                                    // pNext
        flags,                                                      // flags
        (depth == 1) ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,         // imageType
        format,                                                     // format
        { width, height, depth },                                   // extent
        mipLevels,                                                  // mipLevels
        arrayLayers,                                                // arrayLayers
        VK_SAMPLE_COUNT_1_BIT,                                      // samples
        tiling,                                                     // tiling
        usage,                                                      // usage
        VK_SHARING_MODE_EXCLUSIVE,                                  // sharingMode
        1,                                                          // queueFamilyIndexCount
        &m_QueueFamilyIndice.value(),                               // pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED                                   // initialLayout
    };

    vkCreateImage(m_Device, &createInfo, nullptr, &img);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device, img, &memRequirements);

    auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("cannot find suitable memory type");
    };

    VkMemoryAllocateInfo memAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                     // sType
        nullptr,                                                    // pNext
        memRequirements.size,                                       // allocationSize
        findMemoryType(memRequirements.memoryTypeBits,
                        properties)                                 // memoryTypeIndex
    };

    if (vkAllocateMemory(m_Device, &memAllocateInfo, nullptr, &imgMem) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate memory for texture");
    }

    vkBindImageMemory(m_Device, img, imgMem, 0);
}

void VulkanFactory::GenerateMipMaps(VkImage &image, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers) {
    VkCommandBuffer cmdBuff = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,                     // sType
        nullptr,                                                    // pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,                               // srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT,                                // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                       // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                       // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                    // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                    // dstQueueFamilyIndex
        image,                                                      // image
        {
            VK_IMAGE_ASPECT_COLOR_BIT,                          // aspectMask
            0,                                                  // baseMipLevel
            1,                                                  // levelCount
            0,                                                  // baseArrayLayer
            arrayLayers                                         // layerCount
        }// subresourceRange
    };

    int32_t mipWidth = width;
    int32_t mipHeight = height;

    for (uint32_t i = 1; i < mipLevels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuff,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit{
            {
                VK_IMAGE_ASPECT_COLOR_BIT,                      // aspectMask
                i - 1,                                          // mipLevel
                0,                                              // baseArrayLayer
                arrayLayers                                     // layerCount
            },                                                      // srcSubresource
            {
                { 0, 0, 0 },
                { mipWidth, mipHeight, 1 }
            },                                                      // srcOffsets[2]
            {
                VK_IMAGE_ASPECT_COLOR_BIT,                      // aspectMask
                i,                                              // mipLevel
                0,                                              // baseArrayLayer
                arrayLayers                                     // layerCount
            },                                                      // dstSubresource
            {
                { 0, 0, 0 },
                { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 }
            }                                                       // dstOffsets[2]
        };

        vkCmdBlitImage(cmdBuff, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuff,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuff,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    EndSingleTimeCommands(cmdBuff);
}

void VulkanFactory::CreateFramebuffers() {
    m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());
    for (uint32_t i = 0; i < m_SwapChainImageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
            m_SwapChainImageViews[i],
            m_DepthImageView
        };

        VkFramebufferCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,              // sType
            nullptr,                                                // pNext
            0,                                                      // flags
            m_RenderPass,                                           // renderPass
            static_cast<uint32_t>(attachments.size()),              // attachmentCount
            attachments.data(),                                     // pAttachments
            m_SwapChainExtent.width,                                // width
            m_SwapChainExtent.height,                               // height
            1                                                       // layers
        };

        if (vkCreateFramebuffer(m_Device, &createInfo, nullptr, &m_SwapChainFramebuffers[i])) {
            throw std::runtime_error("cannot create framebuffer");
        }
    }
}

void VulkanFactory::AllocateSecondaryCommandBuffer(std::vector<VkCommandBuffer>& cmdBuffers) {
    cmdBuffers.resize(m_SwapChainImageViews.size());
    VkCommandBufferAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        m_CommandPool,                                              // commandPool
        VK_COMMAND_BUFFER_LEVEL_SECONDARY,                          // level
        (uint32_t)cmdBuffers.size()                                 // commandBufferCount
    };

    if (vkAllocateCommandBuffers(m_Device, &allocateInfo, cmdBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate command buffers");
    }
}

void VulkanFactory::AllocateCommandBuffers() {
    m_CommandBuffers.resize(m_SwapChainImageViews.size());
    VkCommandBufferAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        m_CommandPool,                                              // commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                            // level
        (uint32_t)m_CommandBuffers.size()                           // commandBufferCount
    };

    if (vkAllocateCommandBuffers(m_Device, &allocateInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate command buffers");
    }

    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.pNext = nullptr; // Optional
    createInfo.flags = 0; // Reserved for future use, must be 0!

    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = (uint32_t)(m_CommandBuffers.size() * 2);

    VkResult result = vkCreateQueryPool(m_Device, &createInfo, nullptr, &m_QueryPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create time query pool!");
    }
}

void VulkanFactory::CreateCommandPool() {
    VkCommandPoolCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,                 // sType
        nullptr,                                                    // pNext
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,            // flags
        m_QueueFamilyIndice.value()                                 // queueFamilyIndex
    };

    if (vkCreateCommandPool(m_Device, &createInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("cannot create command pool");
    }

    AllocateCommandBuffers();
}

void VulkanFactory::CreateSemaphores() {
    VkSemaphoreCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,                    // sType
        nullptr,                                                    // pNext
        0                                                           // flags
    };
    VkFenceCreateInfo fenceCreateInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,                        // sType
        nullptr,                                                    // pNext
        VK_FENCE_CREATE_SIGNALED_BIT                                // flags
    };
    m_ImageReadySemaphores.resize(m_SwapChainImages.size());
    m_RenderFinishedSemaphores.resize(m_SwapChainImages.size());
    m_CmdBuffFreeFences.resize(m_SwapChainImages.size());
    for (uint32_t i = 0; i < m_ImageReadySemaphores.size(); ++i) {
        if (vkCreateSemaphore(m_Device, &createInfo, nullptr, &m_ImageReadySemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device, &createInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_CmdBuffFreeFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("cannot create semaphore");
        }
    }
}

void VulkanFactory::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                       // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        size,                                                       // size
        usage,                                                      // usage
        VK_SHARING_MODE_EXCLUSIVE,                                  // sharingMode
        1,                                                          // queueFamilyIndexCount
        &m_QueueFamilyIndice.value()                                // pQueueFamilyIndices
    };

    if (vkCreateBuffer(m_Device, &createInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("cannot create vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

    auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("cannot find suitable memory type");
    };

    VkMemoryAllocateFlagsInfo flagsInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,               // sType
        nullptr,                                                    // pNext
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,                      // flags
        0                                                           // deviceMask
    };

    VkMemoryAllocateInfo memAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                     // sType
        &flagsInfo,                                                 // pNext
        memRequirements.size,                                       // allocationSize
        findMemoryType(memRequirements.memoryTypeBits,
                        properties)                                 // memoryTypeIndex
    };

    if (vkAllocateMemory(m_Device, &memAllocateInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate device memory");
    }
    vkBindBufferMemory(m_Device, buffer, memory, 0);
}

void VulkanFactory::CopyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size) {
    VkCommandBuffer cmdBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion = {
        0,                                                          // srcOffset
        0,                                                          // dstOffset
        size                                                        // size
    };
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    EndSingleTimeCommands(cmdBuffer);
}

VkCommandBuffer VulkanFactory::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        m_CommandPool,                                              // commandPool
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,                            // level
        1                                                           // commandBufferCount
    };
    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo cmdBuffbeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,                // sType
        nullptr,                                                    // pNext
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,                // flags
        nullptr                                                     // pInheritanceInfo
    };
    vkBeginCommandBuffer(cmdBuffer, &cmdBuffbeginInfo);

    return cmdBuffer;
}

void VulkanFactory::EndSingleTimeCommands(VkCommandBuffer buffer) {
    vkEndCommandBuffer(buffer);

    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,                              // sType
        nullptr,                                                    // pNext
        0,                                                          // waitSemaphoreCount
        nullptr,                                                    // pWaitSemaphores
        0,                                                          // pWaitDstStageMask
        1,                                                          // commandBufferCount
        &buffer,                                                    // pCommandBuffers
        0,                                                          // signalSemaphoreCount
        nullptr                                                     // pSignalSemaphores
    };
    vkQueueSubmit(m_Queue, 1, &submitInfo, VK_NULL_HANDLE);
    VkResult result = vkQueueWaitIdle(m_Queue);
    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &buffer);
}

void VulkanFactory::CreateTextureSampler(VkSampler& textureSampler) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

    VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,                      // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_FILTER_LINEAR,                                           // magFilter
        VK_FILTER_LINEAR,                                           // minFilter
        VK_SAMPLER_MIPMAP_MODE_LINEAR,                              // mipmapMode
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,                      // addressModeU
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,                      // addressModeV
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,                      // addressModeW
        0.0f,                                                       // mipLodBias
        VK_TRUE,                                                    // anisotropyEnable
        properties.limits.maxSamplerAnisotropy,                     // maxAnisotropy
        VK_FALSE,                                                   // compareEnable
        VK_COMPARE_OP_ALWAYS,                                       // compareOp
        0.0f,                                                       // minLod
        20.0f,                                                      // maxLod
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,                           // borderColor
        VK_FALSE                                                    // unnormalizedCoordinates
    };

    if (vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("cannot create sampler");
    }
}

OffscreenRender &&VulkanFactory::CreateOffscreenRenderer() {
    OffscreenRender render{};

    CreateImage(m_SwapChainExtent.width, m_SwapChainExtent.height, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        render.targetImage, render.targetImageMemory, 1, 0);

    TransitionImageLayout(render.targetImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);

    render.targetImageView = CreateImageView(render.targetImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
    CreateTextureSampler(render.targetSampler);

    std::vector<VkDescriptorSetLayoutBinding> samplerLayoutBinding = { {
        0,                                                          // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        1,                                                          // descriptorCount
        VK_SHADER_STAGE_FRAGMENT_BIT,                               // stageFlags
        nullptr                                                     // pImmutableSamplers
    } };

    CreateDescriptorSetLayout(samplerLayoutBinding, render.descriptorSetLayout);

    std::vector<VkDescriptorPoolSize> samplerPoolSize = { {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // type
        1                                                           // descriptorCount
    } };

    CreateDescriptorPool(samplerPoolSize, render.descriptorPool);

    VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        render.descriptorPool,                                      // descriptorPool
        1,                                                          // descriptorSetCount
        &render.descriptorSetLayout                                 // pSetLayouts
    };

    if (vkAllocateDescriptorSets(m_Device, &allocateInfo, &render.descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate descriptor sets");
    }

    VkDescriptorImageInfo imageInfo = {
        render.targetSampler,                                       // sampler
        render.targetImageView,                                     // imageView
        VK_IMAGE_LAYOUT_GENERAL                                     // imageLayout
    };

    std::array<VkWriteDescriptorSet, 1> writeDescriptorSet{};

    writeDescriptorSet[0] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                     // sType
        nullptr,                                                    // pNext
        render.descriptorSet,                                       // dstSet
        0,                                                          // dstBinding
        0,                                                          // dstArrayElement
        1,                                                          // descriptorCount
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        &imageInfo,                                                 // pImageInfo
        nullptr,                                                    // pBufferInfo
        nullptr                                                     // pTexelBufferView
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);

    return std::move(render);
}

void VulkanFactory::CopyBufferToImage(VkBuffer& srcBuffer, VkImage dstImage, uint32_t width, uint32_t height, uint32_t depth, uint32_t faceNo) {
    VkCommandBuffer cmdBuffer = BeginSingleTimeCommands();

    VkBufferImageCopy region = {
        0,                                                          // bufferOffset
        0,                                                          // bufferRowLength
        0,                                                          // bufferImageHeight
        {
            VK_IMAGE_ASPECT_COLOR_BIT,                          // aspectMask
            0,                                                  // mipLevel
            faceNo,                                             // baseArrayLayer
            1,                                                  // layerCount
        },                                                          // imageSubresource
        {0, 0, 0},                                                  // imageOffset
        {width, height, depth}                                      // imageExtent
    };

    vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(cmdBuffer);
}

void VulkanFactory::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t mipLevels) {
    VkCommandBuffer cmdBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier imgMemoryBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,                     // sType
        nullptr,                                                    // pNext
        0,                                                          // srcAccessMask
        0,                                                          // dstAccessMask
        oldLayout,                                                  // oldLayout
        newLayout,                                                  // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                    // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                    // dstQueueFamilyIndex
        image,                                                      // image
        {
            VK_IMAGE_ASPECT_COLOR_BIT,                          // aspectMask
            0,                                                  // baseMipLevel
            mipLevels,                                          // levelCount
            0,                                                  // baseArrayLayer
            layerCount                                          // layerCount
        }                                                           // subresourceRange
    };

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        imgMemoryBarrier.srcAccessMask = 0;
        imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        imgMemoryBarrier.srcAccessMask = 0;
        imgMemoryBarrier.dstAccessMask = VkAccessFlags();

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("unsuported image layout transition");
    }

    vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &imgMemoryBarrier);

    EndSingleTimeCommands(cmdBuffer);
}

void VulkanFactory::CreateShaderModule(VkShaderModule& shaderModule, const std::string& shaderFilename) {
    auto loadShader = [](std::string filePath, std::vector<char>& shader) {
        std::ifstream fin(filePath, std::ios::ate | std::ios::binary);

        if (!fin.is_open()) {
            throw std::runtime_error("failed to open shader " + filePath);
        }

        size_t fileSize = (size_t)fin.tellg();
        shader.resize(fileSize);
        fin.seekg(0);
        fin.read(shader.data(), fileSize);

        fin.close();
    };

    std::vector<char> shaderCode;
    loadShader(shaderFilename, shaderCode);

    VkShaderModuleCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,            // sType
        nullptr,                                                // pNext
        0,                                                      // flags
        shaderCode.size(),                                      // codeSize
        reinterpret_cast<const uint32_t*>(shaderCode.data()),   // pCode
    };

    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module");
    }
}

void VulkanFactory::CreateGraphicsPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, std::vector<VkPushConstantRange>& pushConstantRanges,
                                                    VkPipelineLayout& pipelineLayout) {
    

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,       // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        2,                                                          // dynamicStateCount
        dynamicStates                                               // pDynamicStates
    };


    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,              // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        1,                                                          // setLayoutCount
        descriptorSetLayouts.data(),                                // pSetLayouts
        static_cast<uint32_t>(pushConstantRanges.size()),           // pushConstantRangeCount
        pushConstantRanges.data()                                   // pPushConstantRanges
    };

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("cannot create pipeline layout");
    }

}

void VulkanFactory::CreateGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, VkPipelineVertexInputStateCreateInfo& vertexInput,
                                            VkPipelineLayout& pipelineLayout, VkPipeline& graphicsPipeline, uint32_t culling, uint32_t depthEnabled) {
    VkViewport viewport = {
        0.0f,                                                       // x
        0.0f,                                                       // y
        (float)m_SwapChainExtent.width,                             // width
        (float)m_SwapChainExtent.height,                            // height
        0.0f,                                                       // minDepth
        1.0f                                                        // maxDepth
    };

    VkRect2D scissor = {
        {0,0},                                                      // offset
        m_SwapChainExtent                                           // extent
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,// sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                        // topology
        VK_FALSE                                                    // primitiveRestartEnable
    };

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,      // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        1,                                                          // viewportCount
        &viewport,                                                  // pViewports
        1,                                                          // scissorCount
        &scissor                                                    // pScissors
    };

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_FALSE,                                                   // depthClampEnable
        VK_FALSE,                                                   // rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,                                       // polygonMode
        culling,                                                    // cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // frontFace
        VK_FALSE,                                                   // depthBiasEnable
        0.0f,                                                       // depthBiasConstantFactor
        0.0f,                                                       // depthBiasClamp
        0.0f,                                                       // depthBiasSlopeFactor
        1.0f,                                                       // lineWidth
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        depthEnabled,                                                    // depthTestEnable
        depthEnabled,                                                    // depthWriteEnable
        VK_COMPARE_OP_LESS,                                         // depthCompareOp
        VK_FALSE,                                                   // depthBoundsTestEnable
        VK_FALSE,                                                   // stencilTestEnable
        {},                                                         // front
        {},                                                         // back
        0.0f,                                                       // minDepthBounds
        1.0f                                                        // maxDepthBounds
    };
    
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,   // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_SAMPLE_COUNT_1_BIT,                                      // rasterizationSamples
        VK_FALSE,                                                   // sampleShadingEnable
        1.0f,                                                       // minSampleShading
        nullptr,                                                    // pSampleMask
        VK_FALSE,                                                   // alphaToCoverageEnable
        VK_FALSE                                                    // alphaToOneEnable
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,                                                   // blendEnable
        VK_BLEND_FACTOR_ONE,                                        // srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,                                       // dstColorBlendFactor
        VK_BLEND_OP_ADD,                                            // colorBlendOp
        VK_BLEND_FACTOR_ONE,                                        // srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,                                       // dstAlphaBlendFactor
        VK_BLEND_OP_ADD,                                            // alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT         // colorWriteMask
    };

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,   // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_FALSE,                                                   // logicOpEnable
        VK_LOGIC_OP_COPY,                                           // logicOp
        1,                                                          // attachmentCount
        &colorBlendAttachmentState,                                 // pAttachments
        { 0.0f, 0.0f, 0.0f, 0.0f }                                  // blendConstants[4]
    };

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,            // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        (uint32_t)(shaderStages.size()),                            // stageCount
        shaderStages.data(),                                        // pStages
        &vertexInput,                                               // pVertexInputState
        &inputAssemblyCreateInfo,                                   // pInputAssemblyState
        nullptr,                                                    // pTessellationState
        &viewportStateCreateInfo,                                   // pViewportState
        &rasterizationStateCreateInfo,                              // pRasterizationState
        &multisampleStateCreateInfo,                                // pMultisampleState
        &depthStencilStateCreateInfo,                               // pDepthStencilState
        &colorBlendStateCreateInfo,                                 // pColorBlendState
        nullptr,                                                    // pDynamicState
        pipelineLayout,                                             // layout
        m_RenderPass,                                               // renderPass
        0,                                                          // subpass
        VK_NULL_HANDLE,                                             // basePipelineHandle
        -1                                                          // basePipelineIndex
    };

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("cannot create graphics pipeline");
    }
}

void VulkanFactory::CreateDescriptorPool(std::vector<VkDescriptorPoolSize>& poolSizes, VkDescriptorPool& descriptorPool) {
    VkDescriptorPoolCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,              // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        static_cast<uint32_t>(m_SwapChainImages.size()),            // maxSets
        static_cast<uint32_t>(poolSizes.size()),                    // poolSizeCount
        poolSizes.data()                                            // pPoolSizes
    };

    if (vkCreateDescriptorPool(m_Device, &createInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("cannot create descriptor pool");
    }
}

void VulkanFactory::CreateTextureDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkImageView& textureImageView,
                                            VkSampler& textureSampler, VkDescriptorSetLayout& layout, VkDescriptorPool& pool) {
    std::vector<VkDescriptorSetLayout> layouts(m_SwapChainImages.size(), layout);
    VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        pool,                                                       // descriptorPool
        static_cast<uint32_t>(m_SwapChainImages.size()),            // descriptorSetCount
        layouts.data()                                              // pSetLayouts
    };
    descriptorSets.resize(m_SwapChainImages.size());

    if (vkAllocateDescriptorSets(m_Device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate descriptor sets");
    }

    for (uint32_t i = 0; i < descriptorSets.size(); ++i) {
        VkDescriptorImageInfo imageInfo = {
            textureSampler,                                       // sampler
            textureImageView,                                     // imageView
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                // imageLayout
        };

        std::array<VkWriteDescriptorSet, 1> writeDescriptorSet{};

        writeDescriptorSet[0] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 // sType
            nullptr,                                                // pNext
            descriptorSets[i],                                      // dstSet
            0,                                                      // dstBinding
            0,                                                      // dstArrayElement
            1,                                                      // descriptorCount
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,              // descriptorType
            &imageInfo,                                             // pImageInfo
            nullptr,                                                // pBufferInfo
            nullptr                                                 // pTexelBufferView
        };

        vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);
    }
}

void VulkanFactory::CreateMultipleTextureDescriptorSets(std::vector<VkDescriptorSet> &descriptorSets, std::vector<VkDescriptorImageInfo>& imageInfos,
                                                        VkDescriptorSetLayout &layout, VkDescriptorPool &pool) {
    std::vector<VkDescriptorSetLayout> layouts(m_SwapChainImages.size(), layout);
    VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        pool,                                                       // descriptorPool
        static_cast<uint32_t>(m_SwapChainImages.size()),            // descriptorSetCount
        layouts.data()                                              // pSetLayouts
    };
    descriptorSets.resize(m_SwapChainImages.size());

    if (vkAllocateDescriptorSets(m_Device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate descriptor sets");
    }

    for (uint32_t i = 0; i < descriptorSets.size(); ++i) {

        std::vector<VkWriteDescriptorSet> writeDescriptorSet{};

        for (uint32_t j = 0; j < imageInfos.size(); ++j) {
            writeDescriptorSet.push_back({
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             // sType
                nullptr,                                            // pNext
                descriptorSets[i],                                  // dstSet
                j,                                                  // dstBinding
                0,                                                  // dstArrayElement
                1,                                                  // descriptorCount
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,          // descriptorType
                &imageInfos[j],                                     // pImageInfo
                nullptr,                                            // pBufferInfo
                nullptr                                             // pTexelBufferView
            });
        }

        vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);
    }
}

void VulkanFactory::CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout& descriptorSetLayout) {
    VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,        // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        static_cast<uint32_t>(bindings.size()),                     // bindingCount
        bindings.data()                                             // pBindings
    };

    vkCreateDescriptorSetLayout(m_Device, &createInfo, nullptr, &descriptorSetLayout);
}

void VulkanFactory::Cleanup() {
    for (auto semaphore : m_ImageReadySemaphores) {
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    }
    for (auto semaphore : m_RenderFinishedSemaphores) {
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    }
    for (auto fence : m_CmdBuffFreeFences) {
        vkDestroyFence(m_Device, fence, nullptr);
    }

    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

    vkDestroySurfaceKHR(m_VkInstance, m_Surface, nullptr);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroyInstance(m_VkInstance, nullptr);
}

VkDeviceAddress VulkanFactory::GetBufferAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = { 
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,               // sType
        nullptr,                                                    // pNext
        buffer                                                      // buffer
    };
    return vkGetBufferDeviceAddress(m_Device, &info);
}

VkDeviceAddress VulkanFactory::GetAccelerationStructureAddress(VkAccelerationStructureKHR as) {
    VkAccelerationStructureDeviceAddressInfoKHR info{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,   // sType
        nullptr,                                                            // pNext
        as                                                                  // accelerationStructure
    };
    return vkGetAccelerationStructureDeviceAddressKHR(m_Device, &info);
}

AccelerationStructure &&VulkanFactory::CreateBLAS(VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexNo, uint32_t primitiveNo) {
    VkDeviceAddress vertexBufferAddress = GetBufferAddress(vertexBuffer);
    VkDeviceAddress indexBufferAddress = GetBufferAddress(indexBuffer);

    VkAccelerationStructureGeometryTrianglesDataKHR asGeometryTrianglesData{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,   // sType
        nullptr,                                                                // pNext
        VK_FORMAT_R32G32B32_SFLOAT,                                             // vertexFormat
        { vertexBufferAddress },                                                // vertexData
        sizeof(Vertex),                                                         // vertexStride
        vertexNo,                                                               // maxVertex
        VK_INDEX_TYPE_UINT32,                                                   // indexType
        { indexBufferAddress },                                                 // indexData
        {}                                                                      // transformData
    };

    VkAccelerationStructureGeometryKHR asGeometry{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,      // sType
        nullptr,                                                    // pNext
        VK_GEOMETRY_TYPE_TRIANGLES_KHR,                             // geometryType
        { asGeometryTrianglesData },                                // geometry
        VK_GEOMETRY_OPAQUE_BIT_KHR                                  // flags
    };

    const VkAccelerationStructureBuildRangeInfoKHR *asBuildRangeInfo = new VkAccelerationStructureBuildRangeInfoKHR{
        primitiveNo,                                                // primitiveCount
        0,                                                          // primitiveOffset
        0,                                                          // firstVertex
        0                                                           // transformOffset
    };

    VkAccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,   // sType
        nullptr,                                                            // pNext
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,                    // type
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,          // flags
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,                     // mode
        {},                                                                 // srcAccelerationStructure
        {},                                                                 // dstAccelerationStructure
        1,                                                                  // geometryCount
        &asGeometry,                                                        // pGeometries
        nullptr,                                                            // ppGeometries
        {}                                                                  // scratchData
    };

    VkAccelerationStructureBuildSizesInfoKHR asBuildSizesInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,  // sType
        nullptr,                                                        // pNext
        0,                                                              // accelerationStructureSize
        0,                                                              // updateScratchSize
        0                                                               // buildScratchSize
    };

    VkResult res{};

    vkGetAccelerationStructureBuildSizesKHR(m_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildGeometryInfo, &primitiveNo, &asBuildSizesInfo);

    VkBuffer scratchBuffer{};
    VkDeviceMemory scratchMemory{};
    CreateBuffer(asBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        0, scratchBuffer, scratchMemory);

    VkBufferDeviceAddressInfo bufferInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,               // sType
        nullptr,                                                    // pNext
        scratchBuffer                                               // buffer
    };
    VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(m_Device, &bufferInfo);

    AccelerationStructure blas{};

    CreateBuffer(asBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0, blas.buffer, blas.memory);

    VkAccelerationStructureCreateInfoKHR asCreateInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,   // sType
        nullptr,                                                    // pNext
        0,                                                          // createFlags
        blas.buffer,                                                // buffer
        0,                                                          // offset
        asBuildSizesInfo.accelerationStructureSize,                 // size
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,            // type
        0                                                           // deviceAddress
    };

    res = vkCreateAccelerationStructureKHR(m_Device, &asCreateInfo, nullptr, &blas.as);

    asBuildGeometryInfo.dstAccelerationStructure = blas.as;
    asBuildGeometryInfo.scratchData.deviceAddress = scratchAddress;

    VkCommandBuffer cmdBuff = BeginSingleTimeCommands();

    // only one creation for now, creating more needs pipeline barrier
    vkCmdBuildAccelerationStructuresKHR(cmdBuff, 1, &asBuildGeometryInfo, &asBuildRangeInfo);

    EndSingleTimeCommands(cmdBuff);

    vkFreeMemory(m_Device, scratchMemory, nullptr);

    return std::move(blas);
}

AccelerationStructure &&VulkanFactory::CreateTLAS(glm::mat4 transformMatrix, VkDeviceAddress BLASAddress, uint32_t primitiveCount) {
    VkTransformMatrixKHR matrix;
    memcpy(&matrix, &transformMatrix, sizeof(VkTransformMatrixKHR));
    VkAccelerationStructureInstanceKHR asInstance{
        matrix,                                                     // transform
        0,                                                          // instanceCustomIndex
        0xFF,                                                       // mask
        0,                                                          // instanceShaderBindingTableRecordOffset
        VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,  // flags
        BLASAddress                                                 // accelerationStructureReference
    };

    VkCommandBuffer cmdBuff = BeginSingleTimeCommands();

    VkBuffer instanceBuffer{};
    VkDeviceMemory instanceMemory{};
    CreateBuffer(sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        0, instanceBuffer, instanceMemory);

    vkCmdUpdateBuffer(cmdBuff, instanceBuffer, 0, sizeof(VkAccelerationStructureInstanceKHR), &asInstance);

    VkDeviceAddress instanceBufferAddress = GetBufferAddress(instanceBuffer);

    VkMemoryBarrier barrier{
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,                           // sType;
        nullptr,                                                    // pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,                               // srcAccessMask;
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR              // dstAccessMask;
    };

    vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    VkAccelerationStructureGeometryInstancesDataKHR asGeometryInstances{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,   // sType
        nullptr,                                                                // pNext
        VK_FALSE,                                                               // arrayOfPointers
        instanceBufferAddress                                                   // data
    };

    VkAccelerationStructureGeometryKHR asGeometry{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,      // sType
        nullptr,                                                    // pNext
        VK_GEOMETRY_TYPE_INSTANCES_KHR,                             // geometryType
        {},                                                         // geometry
        0                                                           // flags
    };
    asGeometry.geometry.instances = asGeometryInstances; // for some reason it didn't let me do it in previous call

    VkAccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,   // sType
        nullptr,                                                            // pNext
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,                       // type
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,          // flags
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,                     // mode
        VK_NULL_HANDLE,                                                     // srcAccelerationStructure
        VK_NULL_HANDLE,                                                     // dstAccelerationStructure
        1,                                                                  // geometryCount
        &asGeometry,                                                        // pGeometries
        nullptr,                                                            // ppGeometries
        0                                                                   // scratchData
    };

    VkAccelerationStructureBuildSizesInfoKHR asBuildSizesInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,  // sType
        nullptr,                                                        // pNext
        0,                                                              // accelerationStructureSize
        0,                                                              // updateScratchSize
        0                                                               // buildScratchSize
    };
    uint32_t count{ 1 };
    vkGetAccelerationStructureBuildSizesKHR(m_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildGeometryInfo, &count, &asBuildSizesInfo);

    AccelerationStructure tlas{};

    CreateBuffer(asBuildSizesInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0, tlas.buffer, tlas.memory);

    VkAccelerationStructureCreateInfoKHR asCreateInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,   // sType
        nullptr,                                                    // pNext
        0,                                                          // createFlags
        tlas.buffer,                                                // buffer
        0,                                                          // offset
        asBuildSizesInfo.accelerationStructureSize,                 // size
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,               // type
        0                                                           // deviceAddress
    };

    vkCreateAccelerationStructureKHR(m_Device, &asCreateInfo, nullptr, &tlas.as);

    VkBuffer scratchBuffer{};
    VkDeviceMemory scratchMemory{};
    CreateBuffer(asBuildSizesInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        0, scratchBuffer, scratchMemory);

    VkDeviceAddress scratchAddress = GetBufferAddress(scratchBuffer);
    
    asBuildGeometryInfo.scratchData.deviceAddress = scratchAddress;
    asBuildGeometryInfo.dstAccelerationStructure = tlas.as;

    const VkAccelerationStructureBuildRangeInfoKHR *asBuildRangeInfo = new VkAccelerationStructureBuildRangeInfoKHR{
        1,                                                          // primitiveCount
        0,                                                          // primitiveOffset
        0,                                                          // firstVertex
        0                                                           // transformOffset
    };

    vkCmdBuildAccelerationStructuresKHR(cmdBuff, 1, &asBuildGeometryInfo, &asBuildRangeInfo);

    EndSingleTimeCommands(cmdBuff);

    vkFreeMemory(m_Device, scratchMemory, nullptr);
    vkFreeMemory(m_Device, instanceMemory, nullptr);

    return std::move(tlas);
}

void VulkanFactory::CreateRtDescriptorSets(AccelerationStructure tlas, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool,
                                            std::vector<VkDescriptorSet>& descriptorSets, std::vector<VkImageView> &imageViews) {
    std::vector<VkDescriptorSetLayout> layouts(m_SwapChainImages.size(), descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        descriptorPool,                                             // descriptorPool
        static_cast<uint32_t>(imageViews.size()),                   // descriptorSetCount
        layouts.data()                                              // pSetLayouts
    };
    descriptorSets.resize(imageViews.size());

    if (vkAllocateDescriptorSets(m_Device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate descriptor sets");
    }

    for (uint32_t i = 0; i < descriptorSets.size(); ++i) {
        VkWriteDescriptorSetAccelerationStructureKHR asDescriptorWrite{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,  // sType
            nullptr,                                                            // pNext
            1,                                                                  // accelerationStructureCount
            &tlas.as                                                            // pAccelerationStructures
        };

        VkDescriptorImageInfo imageInfo = {
            {},                                                     // sampler
            imageViews[i],                                          // imageView
            VK_IMAGE_LAYOUT_GENERAL                                 // imageLayout
        };

        std::array<VkWriteDescriptorSet, 2> writeDescriptorSet{};

        writeDescriptorSet[0] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 // sType
            &asDescriptorWrite,                                     // pNext
            descriptorSets[i],                                      // dstSet
            0,                                                      // dstBinding
            0,                                                      // dstArrayElement
            1,                                                      // descriptorCount
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,          // descriptorType
            nullptr,                                                // pImageInfo
            nullptr,                                                // pBufferInfo
            nullptr                                                 // pTexelBufferView
        };

        writeDescriptorSet[1] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 // sType
            nullptr,                                                // pNext
            descriptorSets[i],                                      // dstSet
            1,                                                      // dstBinding
            0,                                                      // dstArrayElement
            1,                                                      // descriptorCount
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                       // descriptorType
            &imageInfo,                                             // pImageInfo
            nullptr,                                                // pBufferInfo
            nullptr                                                 // pTexelBufferView
        };

        vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);
    }
}

void VulkanFactory::UpdateRtDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets) {
    for (uint32_t i = 0; i < descriptorSets.size(); ++i) {
        VkDescriptorImageInfo imageInfo = {
            {},                                                     // sampler
            m_SwapChainImageViews[i],                               // imageView
            VK_IMAGE_LAYOUT_GENERAL                                 // imageLayout
        };

        std::array<VkWriteDescriptorSet, 1> writeDescriptorSet{};
        writeDescriptorSet[0] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 // sType
            nullptr,                                                // pNext
            descriptorSets[i],                                      // dstSet
            1,                                                      // dstBinding
            0,                                                      // dstArrayElement
            1,                                                      // descriptorCount
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                       // descriptorType
            &imageInfo,                                             // pImageInfo
            nullptr,                                                // pBufferInfo
            nullptr                                                 // pTexelBufferView
        };

        vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);
    }
}

void VulkanFactory::CreateRtPipeline(const std::vector<VkDescriptorSetLayout>& rtDescSetLayouts, VkPipelineLayout& pipelineLayout, VkPipeline& rtPipeline, std::vector<VkRayTracingShaderGroupCreateInfoKHR> &shaderGroups) {
    enum StageIndices {
        eRaygen,
        eMiss,
        eMiss2,
        eMiss3,
        eClosestHit,
        eShaderGroupCount
    };

    std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.pName = "main";

    VkShaderModule raygenShaderModule, missShaderModule, miss2ShaderModule, miss3ShaderModule, chShaderModule;
    CreateShaderModule(raygenShaderModule, "shaders/raygen.spv");
    CreateShaderModule(missShaderModule, "shaders/miss.spv");
    CreateShaderModule(miss2ShaderModule, "shaders/shadow.spv");
    CreateShaderModule(miss3ShaderModule, "shaders/rayreflection.spv");
    CreateShaderModule(chShaderModule, "shaders/chit.spv");

    stage.module = raygenShaderModule;
    stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[eRaygen] = stage;
    
    stage.module = missShaderModule;
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eMiss] = stage;
    
    stage.module = miss2ShaderModule;
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eMiss2] = stage;

    stage.module = miss3ShaderModule;
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eMiss3] = stage;

    stage.module = chShaderModule;
    stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[eClosestHit] = stage;

    VkRayTracingShaderGroupCreateInfoKHR rtShaderGroup{
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, // sType
        nullptr,                                                    // pNext
        VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR,              // type
        VK_SHADER_UNUSED_KHR,                                       // generalShader
        VK_SHADER_UNUSED_KHR,                                       // closestHitShader
        VK_SHADER_UNUSED_KHR,                                       // anyHitShader
        VK_SHADER_UNUSED_KHR,                                       // intersectionShader
        nullptr                                                     // pShaderGroupCaptureReplayHandle
    };

    rtShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rtShaderGroup.generalShader = eRaygen;
    shaderGroups.push_back(rtShaderGroup);

    rtShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rtShaderGroup.generalShader = eMiss;
    shaderGroups.push_back(rtShaderGroup);

    rtShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rtShaderGroup.generalShader = eMiss2;
    shaderGroups.push_back(rtShaderGroup);

    rtShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rtShaderGroup.generalShader = eMiss3;
    shaderGroups.push_back(rtShaderGroup);

    rtShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    rtShaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
    rtShaderGroup.closestHitShader = eClosestHit;
    shaderGroups.push_back(rtShaderGroup);

    VkPushConstantRange pushConstants = {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                        // stageFlags
        0,                                                          // offset
        sizeof(RtPushConstants)                                     // size
    };

    VkPipelineLayoutCreateInfo layoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,              // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        (uint32_t)(rtDescSetLayouts.size()),                        // setLayoutCount
        rtDescSetLayouts.data(),                                    // pSetLayouts
        1,                                                          // pushConstantRangeCount
        &pushConstants                                              // pPushConstantRanges
    };

    if (vkCreatePipelineLayout(m_Device, &layoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("cannot create pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,     // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        static_cast<uint32_t>(stages.size()),                       // stageCount
        stages.data(),                                              // pStages
        static_cast<uint32_t>(shaderGroups.size()),                 // groupCount
        shaderGroups.data(),                                        // pGroups
        2,                                                          // maxPipelineRayRecursionDepth
        nullptr,                                                    // pLibraryInfo
        nullptr,                                                    // pLibraryInterface
        nullptr,                                                    // pDynamicState
        pipelineLayout,                                             // layout
        VK_NULL_HANDLE,                                             // basePipelineHandle
        0                                                           // basePipelineIndex
    };

    vkCreateRayTracingPipelinesKHR(m_Device, {}, {}, 1, &pipelineCreateInfo, nullptr, &rtPipeline);

    for (auto &s : stages) {
        vkDestroyShaderModule(m_Device, s.module, nullptr);
    }
}

template <class integral>
constexpr integral alignUp(integral x, size_t a) noexcept {
    return integral((x + (integral(a) - 1)) & ~integral(a - 1));
}

void VulkanFactory::CreateShaderBindingTable(VkPipeline& rtPipeline, VkStridedDeviceAddressRegionKHR& rgenRegion, VkStridedDeviceAddressRegionKHR& missRegion,
                                VkStridedDeviceAddressRegionKHR& hitRegion, VkStridedDeviceAddressRegionKHR& callRegion, VkBuffer& sbtBuffer, VkDeviceMemory& sbtMemory) {
    uint32_t missCount{ 3 };
    uint32_t hitCount{ 1 };
    uint32_t handleCount{ 1 + missCount + hitCount };

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 deviceProperties2 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,        // sType
        &rtProperties,                                         // pNext
        {}                                                     // properties
    };
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &deviceProperties2);

    uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    uint32_t handleSizeAligned = alignUp(handleSize, rtProperties.shaderGroupHandleAlignment);

    rgenRegion.size = rgenRegion.stride = alignUp(handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
    missRegion.stride = handleSizeAligned;
    missRegion.size = alignUp(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
    hitRegion.stride = handleSizeAligned;
    hitRegion.size = alignUp(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

    uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);

    if (vkGetRayTracingShaderGroupHandlesKHR(m_Device, rtPipeline, 0, handleCount, dataSize, handles.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot create rt shader groups");
    }

    VkDeviceSize sbtSize = rgenRegion.size + missRegion.size + hitRegion.size + callRegion.size;
    
    CreateBuffer(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sbtBuffer, sbtMemory);

    VkDeviceAddress sbtAddress = GetBufferAddress(sbtBuffer);
    rgenRegion.deviceAddress = sbtAddress;
    missRegion.deviceAddress = sbtAddress + rgenRegion.size;
    hitRegion.deviceAddress = sbtAddress + rgenRegion.size + missRegion.size;

    auto getHandle = [&](uint32_t i) { return handles.data() + i * handleSize; };

    uint8_t *pSBTBuffer{ nullptr };
    vkMapMemory(m_Device, sbtMemory, 0, sbtSize, 0, reinterpret_cast<void**>(&pSBTBuffer));
    uint8_t *pData{ pSBTBuffer };
    uint32_t handleIdx{ 0 };

    memcpy(pData, getHandle(handleIdx++), handleSize);

    pData = pSBTBuffer + rgenRegion.size;
    for (uint32_t i = 0; i < missCount; ++i) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += missRegion.stride;
    }

    pData = pSBTBuffer + rgenRegion.size + missRegion.size;
    for (uint32_t i = 0; i < hitCount; ++i) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += hitRegion.stride;
    }

    vkUnmapMemory(m_Device, sbtMemory);
}

void VulkanFactory::TraceRays(VkCommandBuffer cmdBuff, const VkStridedDeviceAddressRegionKHR *rgenRegion, const VkStridedDeviceAddressRegionKHR *missRegion, const VkStridedDeviceAddressRegionKHR *hitRegion, const VkStridedDeviceAddressRegionKHR *callRegion) {
    vkCmdTraceRaysKHR(cmdBuff, rgenRegion, missRegion, hitRegion, callRegion, m_SwapChainExtent.width, m_SwapChainExtent.height, 1);
}
