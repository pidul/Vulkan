#pragma once
#include "CommonHeaders.h"
#include "Vertex.h"
#include "VulkanFactory.h"

class Application;

class Skybox {
    friend class Application;
public:
    Skybox();
    void Cleanup();
    void UpdateWindowSize();

    VkCommandBuffer GetCommandBuffer(uint32_t index) {
        return m_CommandBuffers[index];
    }

    uint32_t GetIndicesCount() {
        return static_cast<uint32_t>(m_Indices.size());
    }

    VkCommandBuffer* Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo);

private:
    glm::vec3 m_Rotation;

    VulkanFactory* m_VkFactory;

    uint32_t m_Width, m_Height;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkBuffer m_VertexBuffer;
    VkBuffer m_IndexBuffer;
    VkDeviceMemory m_VertexBufferMemory;
    VkDeviceMemory m_IndexBufferMemory;

    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkImage m_TextureImage;
    VkDeviceMemory m_TextureImageMemory;
    VkImageView m_TextureImageView;

    std::vector<Vertex> m_Vertices;

    std::vector<uint32_t> m_Indices;

    void CreateTextureImage(std::vector<std::string>);
    void CreateTextureImageView();
    void LoadModel();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    glm::mat4 GetModelMatrix(float time) {
        glm::mat4 mat(1.0f);
        mat = glm::rotate(mat, time * glm::radians(10.0f), m_Rotation);
        return mat;
    }
};
