#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <array>
#include <optional>
#include <chrono>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

typedef std::optional<uint32_t> QueueFamilyIndices;

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

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    
    static VkVertexInputBindingDescription GetBindingDescription() {
        return {
            0,                                                      // binding
            sizeof(Vertex),                                         // stride
            VK_VERTEX_INPUT_RATE_VERTEX                             // inputRate
        };
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
        attributeDescriptions[0] = {
            0,                                                      // location
            0,                                                      // binding
            VK_FORMAT_R32G32B32_SFLOAT,                             // format
            0                                                       // offset
        };
        attributeDescriptions[1] = {
            1,                                                      // location
            0,                                                      // binding
            VK_FORMAT_R32G32B32_SFLOAT,                             // format
            offsetof(Vertex, color)                                 // offset
        };
        attributeDescriptions[2] = {
            2,                                                      // location
            0,                                                      // binding
            VK_FORMAT_R32G32_SFLOAT,                                // format
            offsetof(Vertex, texCoord)                              // offset
        };
        return attributeDescriptions;
    }
};

class Application {
public:
    void Run();

private:
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

    VkBuffer m_VertexBuffer;
    VkBuffer m_IndexBuffer;
    std::vector<VkBuffer> m_UniformBuffers;
    VkDeviceMemory m_VertexBufferMemory;
    VkDeviceMemory m_IndexBufferMemory;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;

    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkCommandPool m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageReadySemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;

    VkImage m_TextureImage;
    VkDeviceMemory m_TextureImageMemory;
    VkImageView m_TextureImageView;
    VkSampler m_TextureSampler;

    VkImage m_DepthImage;
    VkDeviceMemory m_DepthImageMemory;
    VkImageView m_DepthImageView;

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // 0
        {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},   // 1
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},    // 2
        {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},   // 3
        {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},    // 4
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},     // 5
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},   // 6
        {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}     // 7
    };

    const std::vector<uint16_t> indices = {
        2, 1, 0,
        0, 3, 2,
        2, 3, 4,
        2, 4, 5,
        7, 5, 4,
        6, 7, 4,
        1, 7, 6,
        0, 1, 6,
        2, 5, 7,
        1, 2, 7,
        6, 3, 0,
        3, 6, 4
    };

    void InitWindow();
    static void FramebufferResizeCallback(GLFWwindow*, int, int);
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
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();
    VkImageView CreateImageView(VkImage img, VkFormat format, VkImageAspectFlags aspectMask);
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& img, VkDeviceMemory& imgMem);
    void CreateDepthResources();
    void AllocateCommandBuffers();
    void RecordCommandBuffers();
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer buffer);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void UpdateUniformBuffer(uint32_t currImg);
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