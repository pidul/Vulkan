#pragma once

#include "CommonHeaders.h"
#include "Vertex.h"

const std::vector<const char*> validationLayers = {
#ifdef _DEBUG
    "VK_LAYER_KHRONOS_validation"
#endif
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct AccelerationStructure {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkAccelerationStructureKHR as;
};

struct RtPushConstants {
    bool useLtc;
    float ax;
    float ay;
};

struct OffscreenRender {
    VkImage targetImage;
    VkDeviceMemory targetImageMemory;
    VkImageView targetImageView;
    VkSampler targetSampler;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
};

class VulkanFactory {
private:
    // function pointers
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;

    static VulkanFactory *m_Instance;
    static std::mutex m_Mutex;

    GLFWwindow *m_Window;

    VkInstance m_VkInstance;
    VkPhysicalDevice m_PhysicalDevice;
    QueueFamilyIndices m_QueueFamilyIndice;
    VkDevice m_Device;
    VkQueue m_Queue;
    VkSurfaceKHR m_Surface;

    VkSwapchainKHR m_SwapChain;
    std::vector<VkImage> m_SwapChainImages;
    std::vector<VkImageView> m_SwapChainImageViews;
    VkFormat m_SwapChainImageFormat;
    VkExtent2D m_SwapChainExtent;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;

    VkImage m_DepthImage;
    VkDeviceMemory m_DepthImageMemory;
    VkImageView m_DepthImageView;

    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    VkQueryPool m_QueryPool;
    VkRenderPass m_RenderPass;

    std::vector<VkSemaphore> m_ImageReadySemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_CmdBuffFreeFences;

    std::vector<double> m_Times;

private:
    VulkanFactory() {};

    void CreateVkInstance();

    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void QueryFunctionPointers();

    void CreateSwapChain();
    void CreateSwapchainImageViews();
    void CreateFramebuffers();

    void CreateCommandPool();
    void AllocateCommandBuffers();

    SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice &physicalDevice);

    void CreateRenderPass();
    void CreateDepthResources();

    void CreateSemaphores();

public:
    ~VulkanFactory();
    VulkanFactory(VulkanFactory &other) = delete;
    void operator=(VulkanFactory &other) = delete;

    static VulkanFactory *GetInstance() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Instance) {
            m_Instance = new VulkanFactory();
        }
        return m_Instance;
    }

    void InitVulkan(GLFWwindow *window);
    void RecreateSwapChain();
    void CleanupSwapChain();
    void Cleanup();

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer buffer);
    void CreateTextureDescriptorSets(std::vector<VkDescriptorSet> &descriptorSets, VkImageView &textureImageView, VkSampler &textureSampler, VkDescriptorSetLayout &layout, VkDescriptorPool &pool);
    void CreateMultipleTextureDescriptorSets(std::vector<VkDescriptorSet> &descriptorSets, std::vector<VkDescriptorImageInfo> &imageInfos, VkDescriptorSetLayout &layout, VkDescriptorPool &pool);
    void CreateDescriptorSets(std::vector<VkDescriptorSet> &descriptorSets, VkDescriptorSetLayout &layout, VkDescriptorPool &pool);
    void AllocateSecondaryCommandBuffer(std::vector<VkCommandBuffer> &cmdBuffer);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &memory);
    void CopyBuffer(VkBuffer &srcBuffer, VkBuffer &dstBuffer, VkDeviceSize size);
    void CreateImage(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage &img, VkDeviceMemory &imgMem, uint32_t arrayLayers, uint32_t flags = 0, uint32_t mipLevels = 1);
    void GenerateMipMaps(VkImage &image, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t mipLevels = 1);
    void CopyBufferToImage(VkBuffer &srcBuffer, VkImage dstImage, uint32_t width, uint32_t height, uint32_t depth, uint32_t faceNo);
    VkImageView CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask, VkImageViewType viewType, uint32_t facesCount, uint32_t mipLevels = 1);
    void CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding> &bindings, VkDescriptorSetLayout &descriptorSetLayout);
    void CreateDescriptorPool(std::vector<VkDescriptorPoolSize> &poolSizes, VkDescriptorPool &descriptorPool);
    void CreateShaderModule(VkShaderModule &shaderModule, const std::string &shaderFilename);
    void CreateGraphicsPipelineLayout(std::vector<VkDescriptorSetLayout> &descriptorSetLayouts, std::vector<VkPushConstantRange> &pushConstantRanges,
        VkPipelineLayout &pipelineLayout);
    void CreateGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPipelineVertexInputStateCreateInfo &vertexInput,
        VkPipelineLayout &pipelineLayout, VkPipeline &graphicsPipeline, uint32_t culling, uint32_t depthEnabled);
    void CreateTextureSampler(VkSampler &textureSampler);
    OffscreenRender &&CreateOffscreenRenderer();

    void FetchRenderTimeResults(uint32_t index) {
#define TIMING_OFF
#ifndef TIMING_OFF // timing
        uint64_t buffer[2];

        VkResult result = vkGetQueryPoolResults(m_Device, m_QueryPool, index * 2, 2, sizeof(uint64_t) * 2, buffer, sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);
        if (result == VK_NOT_READY)
        {
            return;
        } else if (result == VK_SUCCESS)
        {
            m_Times.push_back((double)(buffer[1] - buffer[0]) / 1000000000);
            //std::cout << (double)(buffer[1] - buffer[0]) / 1000000000 << std::endl;
            if (m_Times.size() == 1000) {
                double sum = 0.0;
                double min = 9999.0;
                double max = 0.0;
                for (const auto& time : m_Times) {
                    sum += time;
                    if (min > time) {
                        min = time;
                    }
                    if (max < time) {
                        max = time;
                    }
                }
                std::cout << "avg time: " << sum / 1000 << std::endl;
                std::cout << "avg fps: " << 1000 / sum << std::endl;
                std::cout << "min time: " << min << std::endl;
                std::cout << "best possible fps: " << 1 / min << std::endl;
                std::cout << "max time: " << max << std::endl;
                std::cout << "worst possible fps: " << 1 / max << std::endl;
                throw std::runtime_error("Bedzie tego");
            }
        } else
        {
            throw std::runtime_error("Failed to receive query results!");
        }
#endif
    }

    // RT
    VkDeviceAddress GetBufferAddress(VkBuffer buffer);
    VkDeviceAddress GetAccelerationStructureAddress(VkAccelerationStructureKHR as);
    AccelerationStructure &&CreateBLAS(VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t vertexNo, uint32_t indexNo);
    void CreateTLAS(AccelerationStructure &tlas, VkAccelerationStructureInstanceKHR &asInstance, VkBuildAccelerationStructureFlagsKHR flags, uint32_t primitiveCount, bool update);
    void CreateRtDescriptorSets(AccelerationStructure tlas, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool, std::vector<VkDescriptorSet> &descriptorSets, std::vector<VkImageView> &imageViews);
    void UpdateRtDescriptorSets(std::vector<VkDescriptorSet> &descriptorSets);
    void CreateRtPipeline(const std::vector<VkDescriptorSetLayout> &rtDescSetLayouts, VkPipelineLayout &pipelineLayout, VkPipeline &rtPipeline, std::vector<VkRayTracingShaderGroupCreateInfoKHR> &shaderGroups);
    void CreateShaderBindingTable(VkPipeline &rtPipeline, VkStridedDeviceAddressRegionKHR &rgenRegion, VkStridedDeviceAddressRegionKHR &missRegion,
        VkStridedDeviceAddressRegionKHR &hitRegion, VkStridedDeviceAddressRegionKHR &callRegion, VkBuffer &sbtBuffer, VkDeviceMemory &sbtMemory);
    void TraceRays(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable);

    VkRenderPass &GetRenderPass() { return m_RenderPass; }
    VkFramebuffer &GetFramebuffer(uint32_t index) { return m_SwapChainFramebuffers[index]; }
    VkCommandBuffer &GetCommandBuffer(uint32_t index) { return m_CommandBuffers[index]; }
    VkSemaphore &GetImageReadySemaphore(uint32_t index) { return m_ImageReadySemaphores[index]; }
    VkSemaphore &GetRenderFinishedSemaphore(uint32_t index) { return m_RenderFinishedSemaphores[index]; }
    VkDevice &GetDevice() { return m_Device; }
    VkQueue &GetQueue() { return m_Queue; }
    VkExtent2D &GetExtent() { return m_SwapChainExtent; }
    std::vector<VkImage> &GetSwapchainImages() { return m_SwapChainImages; }
    VkSwapchainKHR &GetSwapchain() { return m_SwapChain; }
    VkFence &GetCmdBuffFence(uint32_t index) { return m_CmdBuffFreeFences[index]; }
    VkQueryPool &GetQueryPool() { return m_QueryPool; }
};
