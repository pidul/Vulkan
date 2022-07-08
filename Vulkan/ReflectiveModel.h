#pragma once
#include "CommonHeaders.h"
#include "Vertex.h"
#include "VulkanFactory.h"
#include "Interfaces.h"

class ReflectiveModel : public DrawableInterface {
public:
    ReflectiveModel(std::vector<std::string> modelFilenames);
    void Cleanup() override;
    void UpdateWindowSize() override;

    void AddInstance(glm::vec3 tv, glm::vec3 sv, glm::vec3 rv, bool rotateFirst) {
        Instance instance(tv, sv, rv, rotateFirst);
        m_Instances.push_back(instance);
    }

    VkCommandBuffer* Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo, glm::mat4& viewMatrix, LightsPositions lp) override;

    void UpdateLightPosition(uint32_t index, glm::vec4 position) override {
        m_Instances[index].UpdatePosition(position);
    }

private:
    class Instance {
        glm::vec3 m_Translation;
        glm::vec3 m_Scale;
        glm::vec3 m_Rotation;
        bool m_RotateFirst;
    public:
        Instance(glm::vec3 tv, glm::vec3 sv, glm::vec3 rv, bool rf) : m_Translation(tv), m_Scale(sv), m_Rotation(rv), m_RotateFirst(rf) {}
        glm::mat4 GetModelMatrix(float time) {
            glm::mat4 mat(1.0f);
            if (m_RotateFirst) {
                mat = glm::rotate(mat, time * glm::radians(10.0f), m_Rotation);
                mat = glm::translate(mat, m_Translation);
                mat = glm::scale(mat, m_Scale);
            }
            else {
                mat = glm::translate(mat, m_Translation);
                mat = glm::rotate(mat, /*time * */glm::radians(90.0f), m_Rotation);
                mat = glm::scale(mat, m_Scale);
            }
            return mat;
        }
        void UpdatePosition(glm::vec4 position) {
            m_Translation = position;
        }
    };

    std::vector<Instance> m_Instances;
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

    void CreateTextureImage(std::vector<std::string> textures);
    void CreateTextureImageView();
    void LoadModel(std::string);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateGraphicsPipeline();
};
