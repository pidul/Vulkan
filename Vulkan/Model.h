#pragma once
#include "CommonHeaders.h"
#include "Vertex.h"
#include "Application.h"

class Application;

class Model {
    friend class Application;
public:
    Model(Application* mother, VkDevice& device, std::string modelFilename, std::string textureFilename);
    void Cleanup();
    void UpdateWindowSize(uint32_t width, uint32_t height);

    VkCommandBuffer GetCommandBuffer(uint32_t index) {
        return m_CommandBuffers[index];
    }

    uint32_t GetIndicesCount() {
        return static_cast<uint32_t>(m_Indices.size());
    }

    void UpdateTranslationVector(glm::vec3 tv) {
        m_Translation = tv;
    }

    void UpdateScaleVector(glm::vec3 sv) {
        m_Scale = sv;
    }

    void UpdateRotationVector(glm::vec3 rv) {
        m_Rotation = rv;
    }

    VkCommandBuffer* Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo, VkPipelineLayout& pipelineLayout, glm::mat4& viewMatrix);

private:
    Application* m_Mother;

    uint32_t m_Width, m_Height;

    VkDevice& m_Device;

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

    glm::vec3 m_Translation;
    glm::vec3 m_Scale;
    glm::vec3 m_Rotation;

    void CreateTextureImage(std::string);
    void CreateTextureImageView();
    void LoadModel(std::string);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    UniformBufferObject UpdateMVPMatrices(glm::mat4& viewMatrix);
};
