#pragma once
#include "CommonHeaders.h"
#include "Vertex.h"
#include "VulkanFactory.h"
#include "Interfaces.h"

struct BufferAddresses {
    VkDeviceAddress vertexAddress;
    VkDeviceAddress indiceAddress;
};

class RaytracedModel{
public:
    RaytracedModel(std::vector<std::string> modelFilenames);
    void Cleanup();
    void UpdateWindowSize();

    void AddInstance(glm::vec3 tv, glm::vec3 sv, glm::vec3 rv) {
        Instance instance(tv, sv, rv);
        m_Instances.push_back(instance);
    }

    void PrepareForRayTracing();

    void Raytrace(VkCommandBuffer cmdBuff, glm::mat4 viewMatrix, uint32_t index, bool useLtc);
    void Postprocess(VkCommandBuffer cmdBuff, uint32_t idx);

private:
    class Instance {
        glm::vec3 m_Translation;
        glm::vec3 m_Scale;
        glm::vec3 m_Rotation;
    public:
        Instance(glm::vec3 tv, glm::vec3 sv, glm::vec3 rv) : m_Translation(tv), m_Scale(sv), m_Rotation(rv) {}
        glm::mat4 GetModelMatrix() {
            glm::mat4 mat(1.0f);
            mat = glm::translate(mat, m_Translation);
            mat = glm::scale(mat, m_Scale);
            //mat = glm::rotate(mat, glm::radians(90.0f), m_Rotation);
            return glm::inverse(mat);
        }
    };

    std::vector<Instance> m_Instances;
    VulkanFactory *m_VkFactory;

    uint32_t m_Width, m_Height;

    VkBuffer m_VertexBuffer;
    VkDeviceMemory m_VertexBufferMemory;
    VkBuffer m_IndexBuffer;
    VkDeviceMemory m_IndexBufferMemory;
    VkBuffer m_UniformBuffer;
    VkDeviceMemory m_UniformBufferMemory;

    VkImage m_TextureImage;
    VkDeviceMemory m_TextureImageMemory;
    VkImageView m_TextureImageView;
    VkSampler m_TextureSampler;

    std::array<VkImage, 3> m_LTCImage;
    std::array <VkDeviceMemory, 3> m_LTCImageMemory;
    std::array <VkImageView, 3> m_LTCImageView;
    std::array <VkSampler, 3> m_LTCSampler;

    VkDescriptorSetLayout m_DescriptorSetLayout;
    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorSetLayout m_RtDescriptorSetLayout;
    VkDescriptorPool m_RtDescriptorPool;
    std::vector<VkDescriptorSet> m_RtDescriptorSets;
    VkDescriptorSetLayout m_SkyboxDescriptorSetLayout;
    VkDescriptorPool m_SkyboxDescriptorPool;
    std::vector<VkDescriptorSet> m_SkyboxDescriptorSets;
    VkDescriptorSetLayout m_LTCDescriptorSetLayout;
    VkDescriptorPool m_LTCDescriptorPool;
    std::vector<VkDescriptorSet> m_LTCDescriptorSets;

    std::vector<BufferAddresses> m_BufferAddresses;
    VkBuffer m_AddressesStorageBuffer;
    VkDeviceMemory m_AddressesStorageBufferMemory;

    std::vector<OffscreenRender> m_OffscreenRenderTargets;

    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    VkPipelineLayout m_RtPipelineLayout;
    VkPipeline m_RtPipeline;

    VkPipelineLayout m_PostPipelineLayout;
    VkPipeline m_PostPipeline;

    AccelerationStructure m_Blas;
    AccelerationStructure m_Tlas;
    VkBuffer m_RtSBTBuffer;
    VkDeviceMemory m_RtSBTBufferMemory;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups{};
    VkStridedDeviceAddressRegionKHR m_RgenRegion{};
    VkStridedDeviceAddressRegionKHR m_MissRegion{};
    VkStridedDeviceAddressRegionKHR m_HitRegion{};
    VkStridedDeviceAddressRegionKHR m_CallRegion{};

    RtPushConstants m_RtPC{
        glm::vec3{0.0, -13.0, 0.0},
        100,
        false,
        0.9f,
        0.9f
    };

    void LoadModel(std::string);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateTextureImage(std::vector<std::string>);
    void CreateLTCImage();
    void CreateRtPipeline();
    void CreateUniformBuffer();
    void UpdateUniformBuffer(VkCommandBuffer cmdBuff, RtUniformBufferObject& ubo);
    void CreatePostPipeline();
};
