#pragma once

#include "CommonHeaders.h"
#include "Vertex.h"
#include "Model.h"
#include "Camera.h"

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

class Model;

class Application {
    friend class Model;

public:
    void Run();

private:
    std::vector<Model> m_Models;
    Camera m_Camera;

    bool framebufferResized = false;
    GLFWwindow* m_Window;

    VkInstance m_Instance;
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

    VkRenderPass m_RenderPass;
    VkDescriptorSetLayout m_DescriptorSetLayout;
    VkPipelineLayout m_GraphicsPipelineLayout;
    VkPipeline m_GraphicsPipeline;

    VkSampler m_TextureSampler;

    VkDescriptorPool m_DescriptorPool;
    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageReadySemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;

    VkImage m_DepthImage;
    VkDeviceMemory m_DepthImageMemory;
    VkImageView m_DepthImageView;

    void InitWindow();
    static void FramebufferResizeCallback(GLFWwindow*, int, int);
    static void KeyboardInputCallback(GLFWwindow*, int, int, int, int);
    static void MouseInputCallback(GLFWwindow*, double, double);
    void InitVulkan();
    void CreateInstance();

    void PickPhysicalDevice();
    void CreateLogicalDevice();
    
    void CreateSwapChain();
    void RecreateSwapChain();
    void CreateImageViews();
    
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
    
    void CreateCommandPool();

    void CreateTextureSampler();
    VkImageView CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask);
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& img, VkDeviceMemory& imgMem);
    void CreateDepthResources();
    void AllocateCommandBuffers();
    void AllocateSecondaryCommandBuffer(std::vector<VkCommandBuffer>& cmdBuffer);
    void RecordCommandBuffers(uint32_t index);
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer buffer);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CreateDescriptorPool();
    void CreateDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkImageView& textureImageView);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void CopyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
    void CopyBufferToImage(VkBuffer& srcBuffer, VkImage dstImage, uint32_t width, uint32_t height);
    void CreateSemaphores();
    void MainLoop();
    void DrawFrame();
    void CleanupSwapChain();
    void Cleanup();

    SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice&);
};