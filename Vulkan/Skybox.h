#pragma once
#include "CommonHeaders.h"
#include "Vertex.h"
#include "VulkanFactory.h"
#include "Interfaces.h"

class Application;

class Skybox : public DrawableInterface {
public:
    Skybox();
    void Cleanup() override;
    void UpdateLightPosition(uint32_t, glm::vec4) { assert(0); };
    void UpdateWindowSize() override;

    VkCommandBuffer* Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo, glm::mat4& viewMatrix, LightsPositions lp) override;

private:
    VulkanFactory* m_VkFactory;

    uint32_t m_Width, m_Height;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkBuffer m_VertexBuffer;
    VkBuffer m_IndexBuffer;
    VkDeviceMemory m_VertexBufferMemory;
    VkDeviceMemory m_IndexBufferMemory;

    VkDescriptorSetLayout m_DescriptorSetLayout;
    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkImage m_TextureImage;
    VkDeviceMemory m_TextureImageMemory;
    VkImageView m_TextureImageView;
    VkSampler m_TextureSampler;

    std::vector<Vertex> m_Vertices;

    std::vector<uint32_t> m_Indices;

    VkPipelineLayout m_GraphicsPipelineLayout;
    VkPipeline m_GraphicsPipeline;

    void CreateTextureImage(std::vector<std::string>);
    void CreateTextureImageView();
    void LoadModel();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateGraphicsPipeline();
    glm::mat4 GetModelMatrix() {
        static glm::mat4 mat(1.0f);
        return mat;
    }
};
