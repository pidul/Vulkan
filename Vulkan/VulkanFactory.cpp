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
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateSwapchainImageViews();
    CreateDescriptorSetLayout();
    CreateCommandPool();
    CreateTextureSampler();
    CreateDepthResources();
    CreateRenderPass();
    CreateFramebuffers();
    CreateGraphicsPipeline();
    CreateDescriptorPool();
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
        VK_API_VERSION_1_2                                          // apiVersion
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
            if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT) {
                indices = i;
                break;
            }
            ++i;
        }

        return indices;
    };

    auto isDeviceSuitable = [&](const VkPhysicalDevice& physicalDevice) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

        QueueFamilyIndices indice = findQueueFamilies(physicalDevice);

        uint32_t extensionsCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, nullptr);

        std::vector<VkExtensionProperties> extProperties(extensionsCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, extProperties.data());

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& ext : extProperties) {
            requiredExtensions.erase(ext.extensionName);
        }

        bool swapchainGood = false;
        if (requiredExtensions.empty()) {
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
            swapchainGood = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        if (indice.has_value()) {
            m_QueueFamilyIndice = indice;
            return requiredExtensions.empty() && swapchainGood &&
                supportedFeatures.samplerAnisotropy &&
                deviceProperties.limits.maxPushConstantsSize > sizeof(UniformBufferObject);
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

    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                       // sType
        nullptr,                                                    // pNext
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

void VulkanFactory::RecreateSwapChain() {
    vkDeviceWaitIdle(m_Device);

    CleanupSwapChain();

    CreateSwapChain();
    CreateSwapchainImageViews();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateDepthResources();
    CreateFramebuffers();
    CreateDescriptorPool();
    AllocateCommandBuffers();
}

void VulkanFactory::CleanupSwapChain() {
    vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
    vkDestroyImage(m_Device, m_DepthImage, nullptr);
    vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

    vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

    for (auto framebuffer : m_SwapChainFramebuffers) {
        vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
    }

    vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());

    vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    vkDestroyPipelineLayout(m_Device, m_GraphicsPipelineLayout, nullptr);
    for (auto imgView : m_SwapChainImageViews) {
        vkDestroyImageView(m_Device, imgView, nullptr);
    }
    vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

VkImageView VulkanFactory::CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask) {
    VkImageViewCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,                   // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        img,                                                        // image
        VK_IMAGE_VIEW_TYPE_2D,                                      // viewType
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
            1,                                                  // levelCount
            0,                                                  // baseArrayLayer
            1,                                                  // layerCount
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
        m_SwapChainImageViews[i] = CreateImageView(m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanFactory::CreateDepthResources(){
    CreateImage(m_SwapChainExtent.width, m_SwapChainExtent.height, VK_FORMAT_D24_UNORM_S8_UINT,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory, 1);
    m_DepthImageView = CreateImageView(m_DepthImage, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanFactory::CreateImage(uint32_t width, uint32_t height, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties, VkImage& img,
    VkDeviceMemory& imgMem, uint32_t arrayLayers) {
    VkImageCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                        // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_IMAGE_TYPE_2D,                                           // imageType
        format,                                                     // format
        { width, height, 1 },                                       // extent
        1,                                                          // mipLevels
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
    m_ImageReadySemaphores.resize(m_SwapChainImages.size());
    m_RenderFinishedSemaphores.resize(m_SwapChainImages.size());
    for (uint32_t i = 0; i < m_ImageReadySemaphores.size(); ++i) {
        if (vkCreateSemaphore(m_Device, &createInfo, nullptr, &m_ImageReadySemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device, &createInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
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

    VkMemoryAllocateInfo memAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,                     // sType
        nullptr,                                                    // pNext
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
    vkQueueWaitIdle(m_Queue);
    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &buffer);
}

void VulkanFactory::CreateTextureSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

    VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,                      // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_FILTER_LINEAR,                                           // magFilter
        VK_FILTER_LINEAR,                                           // minFilter
        VK_SAMPLER_MIPMAP_MODE_LINEAR,                              // mipmapMode
        VK_SAMPLER_ADDRESS_MODE_REPEAT,                             // addressModeU
        VK_SAMPLER_ADDRESS_MODE_REPEAT,                             // addressModeV
        VK_SAMPLER_ADDRESS_MODE_REPEAT,                             // addressModeW
        0.0f,                                                       // mipLodBias
        VK_TRUE,                                                    // anisotropyEnable
        properties.limits.maxSamplerAnisotropy,                     // maxAnisotropy
        VK_FALSE,                                                   // compareEnable
        VK_COMPARE_OP_ALWAYS,                                       // compareOp
        0.0f,                                                       // minLod
        0.0f,                                                       // maxLod
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,                           // borderColor
        VK_FALSE                                                    // unnormalizedCoordinates
    };

    if (vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &m_TextureSampler) != VK_SUCCESS) {
        throw std::runtime_error("cannot create sampler");
    }
}

void VulkanFactory::CopyBufferToImage(VkBuffer& srcBuffer, VkImage dstImage, uint32_t width, uint32_t height) {
    VkCommandBuffer cmdBuffer = BeginSingleTimeCommands();

    VkBufferImageCopy region = {
        0,                                                          // bufferOffset
        0,                                                          // bufferRowLength
        0,                                                          // bufferImageHeight
        {
            VK_IMAGE_ASPECT_COLOR_BIT,                          // aspectMask
            0,                                                  // mipLevel
            0,                                                  // baseArrayLayer
            1,                                                  // layerCount
        },                                                          // imageSubresource
        {0, 0, 0},                                                  // imageOffset
        {width, height, 1}                                          // imageExtent
    };

    vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(cmdBuffer);
}

void VulkanFactory::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
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
            1,                                                  // levelCount
            0,                                                  // baseArrayLayer
            1                                                   // layerCount
        }                                                           // subresourceRange
    };

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        imgMemoryBarrier.srcAccessMask = 0;
        imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
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


void VulkanFactory::CreateGraphicsPipeline() {
    auto loadShader = [](std::string filePath, std::vector<char>& shader) {
        std::ifstream fin(filePath, std::ios::ate | std::ios::binary);

        if (!fin.is_open()) {
            throw std::runtime_error("failed to open shader" + filePath);
        }

        size_t fileSize = (size_t)fin.tellg();
        shader.resize(fileSize);
        fin.seekg(0);
        fin.read(shader.data(), fileSize);

        fin.close();
    };

    std::vector<char> vs, fs;
    loadShader("shaders/vert.spv", vs);
    loadShader("shaders/frag.spv", fs);

    auto createShaderModule = [&](std::vector<char>& shaderCode) {
        VkShaderModuleCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,            // sType
            nullptr,                                                // pNext
            0,                                                      // flags
            shaderCode.size(),                                      // codeSize
            reinterpret_cast<const uint32_t*>(shaderCode.data()),   // pCode
        };
        VkShaderModule module;
        if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module");
        }

        return module;
    };

    VkShaderModule vertexShaderModule = createShaderModule(vs);
    VkShaderModule fragmentShaderModule = createShaderModule(fs);

    VkPipelineShaderStageCreateInfo vsStageCreateInfo = {
         VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,       // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_SHADER_STAGE_VERTEX_BIT,                                 // stage
        vertexShaderModule,                                         // module
        "main",                                                     // pName
        nullptr                                                     // pSpecializationInfo
    };

    VkPipelineShaderStageCreateInfo fsStageCreateInfo = {
         VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,       // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_SHADER_STAGE_FRAGMENT_BIT,                               // stage
        fragmentShaderModule,                                       // module
        "main",                                                     // pName
        nullptr                                                     // pSpecializationInfo
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = { vsStageCreateInfo, fsStageCreateInfo };

    auto inputAttribute = Vertex::GetAttributeDescription();
    auto bindingDescr = Vertex::GetBindingDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        1,                                                          // vertexBindingDescriptionCount
        &bindingDescr,                                              // pVertexBindingDescriptions
        static_cast<uint32_t>(inputAttribute.size()),               // vertexAttributeDescriptionCount
        inputAttribute.data()                                       // pVertexAttributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,// sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                        // topology
        VK_FALSE                                                    // primitiveRestartEnable
    };

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
        VK_CULL_MODE_BACK_BIT,                                      // cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // frontFace
        VK_FALSE,                                                   // depthBiasEnable
        0.0f,                                                       // depthBiasConstantFactor
        0.0f,                                                       // depthBiasClamp
        0.0f,                                                       // depthBiasSlopeFactor
        1.0f,                                                       // lineWidth
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

    std::array<VkPushConstantRange, 2> pushConstantRanges = {};
    pushConstantRanges[0] = {
        VK_SHADER_STAGE_VERTEX_BIT,                                 // stageFlags
        0,                                                          // offset
        sizeof(UniformBufferObject)                                 // size
    };

    pushConstantRanges[1] = {
        VK_SHADER_STAGE_FRAGMENT_BIT,                               // stageFlags
        sizeof(UniformBufferObject),                                // offset
        sizeof(LightsPositions)                                     // size
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,              // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        1,                                                          // setLayoutCount
        &m_DescriptorSetLayout,                                     // pSetLayouts
        static_cast<uint32_t>(pushConstantRanges.size()),           // pushConstantRangeCount
        pushConstantRanges.data()                                   // pPushConstantRanges
    };

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_GraphicsPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("cannot create pipeline layout");
    }

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_TRUE,                                                    // depthTestEnable
        VK_TRUE,                                                    // depthWriteEnable
        VK_COMPARE_OP_LESS,                                         // depthCompareOp
        VK_FALSE,                                                   // depthBoundsTestEnable
        VK_FALSE,                                                   // stencilTestEnable
        {},                                                         // front
        {},                                                         // back
        0.0f,                                                       // minDepthBounds
        1.0f                                                        // maxDepthBounds
    };

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,            // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        2,                                                          // stageCount
        shaderStages,                                               // pStages
        &vertexInputStateCreateInfo,                                // pVertexInputState
        &inputAssemblyCreateInfo,                                   // pInputAssemblyState
        nullptr,                                                    // pTessellationState
        &viewportStateCreateInfo,                                   // pViewportState
        &rasterizationStateCreateInfo,                              // pRasterizationState
        &multisampleStateCreateInfo,                                // pMultisampleState
        &depthStencilStateCreateInfo,                               // pDepthStencilState
        &colorBlendStateCreateInfo,                                 // pColorBlendState
        nullptr,                                                    // pDynamicState
        m_GraphicsPipelineLayout,                                   // layout
        m_RenderPass,                                               // renderPass
        0,                                                          // subpass
        VK_NULL_HANDLE,                                             // basePipelineHandle
        -1                                                          // basePipelineIndex
    };

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("cannot create graphics pipeline");
    }

    vkDestroyShaderModule(m_Device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, fragmentShaderModule, nullptr);
}

void VulkanFactory::CreateDescriptorPool() {
    VkDescriptorPoolSize samplerPoolSize = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                          // type
        static_cast<uint32_t>(m_SwapChainImages.size()) *
            MAX_OBJECTS_ON_SCENE                                    // descriptorCount
    };

    std::array<VkDescriptorPoolSize, 1> poolSizes = { samplerPoolSize };

    VkDescriptorPoolCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,              // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        static_cast<uint32_t>(m_SwapChainImages.size()) *
            MAX_OBJECTS_ON_SCENE,                                   // maxSets
        static_cast<uint32_t>(poolSizes.size()),                    // poolSizeCount
        poolSizes.data()                                            // pPoolSizes
    };

    if (vkCreateDescriptorPool(m_Device, &createInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("cannot create descriptor pool");
    }
}

void VulkanFactory::CreateDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkImageView& textureImageView) {
    std::vector<VkDescriptorSetLayout> layouts(m_SwapChainImages.size(), m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        m_DescriptorPool,                                           // descriptorPool
        static_cast<uint32_t>(m_SwapChainImages.size()),            // descriptorSetCount
        layouts.data()                                              // pSetLayouts
    };
    descriptorSets.resize(m_SwapChainImages.size());

    if (vkAllocateDescriptorSets(m_Device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate descriptor sets");
    }

    for (uint32_t i = 0; i < descriptorSets.size(); ++i) {
        VkDescriptorImageInfo imageInfo = {
            m_TextureSampler,                                       // sampler
            textureImageView,                                     // imageView
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                // imageLayout
        };

        std::array<VkWriteDescriptorSet, 1> writeDescriptorSet;

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

void VulkanFactory::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {
        0,                                                          // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        1,                                                          // descriptorCount
        VK_SHADER_STAGE_FRAGMENT_BIT,                               // stageFlags
        nullptr                                                     // pImmutableSamplers
    };

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = { samplerLayoutBinding };

    VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,        // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        static_cast<uint32_t>(bindings.size()),                     // bindingCount
        bindings.data()                                             // pBindings
    };

    vkCreateDescriptorSetLayout(m_Device, &createInfo, nullptr, &m_DescriptorSetLayout);
}

void VulkanFactory::Cleanup() {
    for (auto semaphore : m_ImageReadySemaphores) {
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    }
    for (auto semaphore : m_RenderFinishedSemaphores) {
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    }

    vkDestroySampler(m_Device, m_TextureSampler, nullptr);

    vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);

    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

    vkDestroySurfaceKHR(m_VkInstance, m_Surface, nullptr);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroyInstance(m_VkInstance, nullptr);
}
