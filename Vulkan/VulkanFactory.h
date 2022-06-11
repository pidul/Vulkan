#pragma once

#include "CommonHeaders.h"
#include "Vertex.h"

const std::vector<const char*> validationLayers = {
#ifdef _DEBUG
    "VK_LAYER_KHRONOS_validation"
#endif
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanFactory {
private:
    static VulkanFactory* m_Instance;
    static std::mutex m_Mutex;

    GLFWwindow* m_Window;

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
    VkRenderPass m_RenderPass;

    std::vector<VkSemaphore> m_ImageReadySemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_CmdBuffFreeFences;

private:
    VulkanFactory() {};

    void CreateVkInstance();
    
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    void CreateSwapChain();
    void CreateSwapchainImageViews();
    void CreateFramebuffers();

    void CreateCommandPool();
    void AllocateCommandBuffers();

    SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& physicalDevice);

    void CreateRenderPass();
    void CreateDepthResources();

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer buffer);
    void CreateSemaphores();
public:
    ~VulkanFactory();
    VulkanFactory(VulkanFactory& other) = delete;
    void operator=(VulkanFactory& other) = delete;

    static VulkanFactory* GetInstance() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Instance) {
            m_Instance = new VulkanFactory();
        }
        return m_Instance;
    }

    void InitVulkan(GLFWwindow* window);
    void RecreateSwapChain();
    void CleanupSwapChain();
    void Cleanup();

    void CreateDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkImageView& textureImageView, VkSampler& textureSampler, VkDescriptorSetLayout& layout, VkDescriptorPool& pool);
    void AllocateSecondaryCommandBuffer(std::vector<VkCommandBuffer>& cmdBuffer);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void CopyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& img, VkDeviceMemory& imgMem, uint32_t arrayLayers, uint32_t flags = 0);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount);
    void CopyBufferToImage(VkBuffer& srcBuffer, VkImage dstImage, uint32_t width, uint32_t height, uint32_t faceNo);
    VkImageView CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask, VkImageViewType viewType, uint32_t facesCount);
    void CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout& descriptorSetLayout);
    void CreateDescriptorPool(std::vector<VkDescriptorPoolSize>& poolSizes, VkDescriptorPool& descriptorPool);
    void CreateShaderModule(VkShaderModule& shaderModule, const std::string& shaderFilename);
    void CreateGraphicsPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, std::vector<VkPushConstantRange>& pushConstantRanges,
                                        VkPipelineLayout& pipelineLayout);
    void CreateGraphicsPipeline(std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, VkPipelineVertexInputStateCreateInfo& vertexInput,
                                VkPipelineLayout& pipelineLayout, VkPipeline& graphicsPipeline, uint32_t culling, uint32_t depthEnabled);
    void CreateTextureSampler(VkSampler& textureSampler);

    VkRenderPass& GetRenderPass() { return m_RenderPass; }
    VkFramebuffer& GetFramebuffer(uint32_t index) { return m_SwapChainFramebuffers[index]; }
    VkCommandBuffer& GetCommandBuffer(uint32_t index) { return m_CommandBuffers[index]; }
    VkSemaphore& GetImageReadySemaphore(uint32_t index) { return m_ImageReadySemaphores[index]; }
    VkSemaphore& GetRenderFinishedSemaphore(uint32_t index) { return m_RenderFinishedSemaphores[index]; }
    VkDevice& GetDevice() { return m_Device; }
    VkQueue& GetQueue() { return m_Queue; }
    VkExtent2D& GetExtent() { return m_SwapChainExtent; }
    std::vector<VkImage>& GetSwapchainImages() { return m_SwapChainImages; }
    VkSwapchainKHR& GetSwapchain() { return m_SwapChain; }
    VkFence& GetCmdBuffFence(uint32_t index) { return m_CmdBuffFreeFences[index]; }
};
