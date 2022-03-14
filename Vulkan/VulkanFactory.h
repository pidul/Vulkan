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

    VkSampler m_TextureSampler;
    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    VkRenderPass m_RenderPass;

    std::vector<VkSemaphore> m_ImageReadySemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;

    VkDescriptorSetLayout m_DescriptorSetLayout;
    VkPipelineLayout m_GraphicsPipelineLayout;
    VkPipeline m_GraphicsPipeline;
    VkDescriptorPool m_DescriptorPool;

private:
    VulkanFactory() {};

    void CreateVkInstance();
    
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    void CreateSwapChain();
    void CreateSwapchainImageViews();
    void CreateFramebuffers();
    void CreateDescriptorSetLayout();
    void CreateGraphicsPipeline();

    void CreateCommandPool();
    void AllocateCommandBuffers();

    SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& physicalDevice);

    void CreateRenderPass();
    void CreateDepthResources();

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer buffer);
    void CreateTextureSampler();
    void CreateSemaphores();
    void CreateDescriptorPool();
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

    void CreateDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkImageView& textureImageView);
    void AllocateSecondaryCommandBuffer(std::vector<VkCommandBuffer>& cmdBuffer);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void CopyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& img, VkDeviceMemory& imgMem, uint32_t arrayLayers);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer& srcBuffer, VkImage dstImage, uint32_t width, uint32_t height);
    VkImageView CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask);

    VkRenderPass& GetRenderPass() { return m_RenderPass; }
    VkFramebuffer& GetFramebuffer(uint32_t index) { return m_SwapChainFramebuffers[index]; }
    VkPipeline& GetGraphicsPipeline() { return m_GraphicsPipeline; }
    VkPipelineLayout& GetPipelineLayout() { return m_GraphicsPipelineLayout; }
    VkCommandBuffer& GetCommandBuffer(uint32_t index) { return m_CommandBuffers[index]; }
    VkSemaphore& GetImageReadySemaphore(uint32_t index) { return m_ImageReadySemaphores[index]; }
    VkSemaphore& GetRenderFinishedSemaphore(uint32_t index) { return m_RenderFinishedSemaphores[index]; }
    //const VkInstance& GetVkInstance() { return m_VkInstance; }
    //VkSurfaceKHR& GetVkSurface() { return m_Surface; }
    //VkPhysicalDevice& GetPhysicalDevice() { return m_PhysicalDevice; }
    //QueueFamilyIndices& GetQueueFamilyIndice() { return m_QueueFamilyIndice; }
    VkDevice& GetDevice() { return m_Device; }
    VkQueue& GetQueue() { return m_Queue; }
    VkExtent2D& GetExtent() { return m_SwapChainExtent; }
    //VkFormat GetFormat() { return m_SwapChainImageFormat; }
    std::vector<VkImage>& GetSwapchainImages() { return m_SwapChainImages; }
    //std::vector<VkImageView>& GetSwapchainImageViews() { return m_SwapChainImageViews; }
    VkSwapchainKHR& GetSwapchain() { return m_SwapChain; }
};
