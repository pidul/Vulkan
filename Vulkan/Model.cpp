#include "Model.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


Model::Model(std::vector<std::string> modelFilenames, std::string textureFilename) :
    m_VertexBuffer(VK_NULL_HANDLE), m_IndexBuffer(VK_NULL_HANDLE),
    m_VertexBufferMemory(VK_NULL_HANDLE), m_IndexBufferMemory(VK_NULL_HANDLE),
    m_TextureImage(VK_NULL_HANDLE), m_TextureImageMemory(VK_NULL_HANDLE), m_TextureImageView(VK_NULL_HANDLE),
    m_VkFactory(VulkanFactory::GetInstance()){
    for (const auto& modelFilename : modelFilenames) {
        LoadModel(modelFilename);
    }
    CreateTextureImage(textureFilename);
    CreateTextureImageView();
    CreateVertexBuffer();
    CreateIndexBuffer();
    UpdateWindowSize();
}

void Model::UpdateWindowSize() {
    m_Width = m_VkFactory->GetExtent().width;
    m_Height = m_VkFactory->GetExtent().height;
    m_VkFactory->CreateDescriptorSets(m_DescriptorSets, m_TextureImageView);
    m_VkFactory->AllocateSecondaryCommandBuffer(m_CommandBuffers);
}

void Model::CreateTextureImage(std::string texPath) {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("cannot load texture");
    }

    VkDeviceSize imageSize = (VkDeviceSize)texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    m_VkFactory->CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory);

    stbi_image_free(pixels);

    m_VkFactory->CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_TextureImage, m_TextureImageMemory, 1);

    m_VkFactory->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_VkFactory->CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    m_VkFactory->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory, nullptr);
}

void Model::CreateTextureImageView() {
    m_TextureImageView = m_VkFactory->CreateImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Model::LoadModel(std::string modelPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warning, error;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, modelPath.c_str())) {
        throw std::runtime_error(warning + error);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex;

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.texcoord_index == -1) {
                vertex.texCoord = { 0.0, 0.0 };
            }
            else {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(m_Vertices.size());
                m_Vertices.push_back(vertex);
            }

            m_Indices.push_back(uniqueVertices[vertex]);
        }
    }
}

void Model::CreateVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Vertices.data(), bufferSize);
    vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory);

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);

    m_VkFactory->CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory, nullptr);
}

void Model::CreateIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Indices.data(), bufferSize);
    vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory);

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);

    m_VkFactory->CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory, nullptr);
}

void Model::Cleanup() {
    vkDestroyBuffer(m_VkFactory->GetDevice(), m_VertexBuffer, nullptr);
    vkDestroyBuffer(m_VkFactory->GetDevice(), m_IndexBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_VertexBufferMemory, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_IndexBufferMemory, nullptr);
    vkDestroyImageView(m_VkFactory->GetDevice(), m_TextureImageView, nullptr);
    vkDestroyImage(m_VkFactory->GetDevice(), m_TextureImage, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_TextureImageMemory, nullptr);
}

VkCommandBuffer* Model::Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo, glm::mat4& viewMatrix, LightsPositions lp) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currTime - startTime).count();

    VkDeviceSize offsets = 0;
    // vkResetCommandBuffer(m_CommandBuffers[index], 0);
    vkBeginCommandBuffer(m_CommandBuffers[index], beginInfo);
    vkCmdBindPipeline(m_CommandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, m_VkFactory->GetGraphicsPipeline());
    vkCmdBindVertexBuffers(m_CommandBuffers[index], 0, 1, &m_VertexBuffer, &offsets);
    vkCmdBindIndexBuffer(m_CommandBuffers[index], m_IndexBuffer, offsets, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(m_CommandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, m_VkFactory->GetPipelineLayout(),
                            0, 1, &m_DescriptorSets[index], 0, nullptr);
    vkCmdPushConstants(m_CommandBuffers[index], m_VkFactory->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(UniformBufferObject), sizeof(lp), &lp);
    for (auto& instance : m_Instances) {
        UniformBufferObject ubo = {};
        ubo.projection = glm::perspective(glm::radians(60.0f), m_Width / (float)m_Height, 0.1f, 20.0f);
        ubo.projection[1][1] *= -1;
        ubo.view = viewMatrix;
        ubo.model = instance.GetModelMatrix(time);
        vkCmdPushConstants(m_CommandBuffers[index], m_VkFactory->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ubo), &ubo);
        vkCmdDrawIndexed(m_CommandBuffers[index], static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);
    }
    vkEndCommandBuffer(m_CommandBuffers[index]);
    return &m_CommandBuffers[index];
}
