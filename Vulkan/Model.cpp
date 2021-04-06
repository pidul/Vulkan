#include "Model.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


Model::Model(Application* mother, VkDevice& device, std::string modelFilename, std::string textureFilename) :
        m_Mother(mother), m_Device(device), m_VertexBuffer(VK_NULL_HANDLE), m_IndexBuffer(VK_NULL_HANDLE),
        m_VertexBufferMemory(VK_NULL_HANDLE), m_IndexBufferMemory(VK_NULL_HANDLE),
        m_TextureImage(VK_NULL_HANDLE), m_TextureImageMemory(VK_NULL_HANDLE), m_TextureImageView(VK_NULL_HANDLE) {
    LoadModel(modelFilename);
    CreateTextureImage(textureFilename);
    CreateTextureImageView();
    CreateVertexBuffer();
    CreateIndexBuffer();
}

void Model::UpdateWindowSize(uint32_t width, uint32_t height) {
    m_Width = width;
    m_Height = height;
    m_Mother->CreateDescriptorSets(m_DescriptorSets, m_TextureImageView);
    m_Mother->AllocateSecondaryCommandBuffer(m_CommandBuffers);
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

    m_Mother->CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    stbi_image_free(pixels);

    m_Mother->CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_TextureImage, m_TextureImageMemory);

    m_Mother->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_Mother->CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    m_Mother->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void Model::CreateTextureImageView() {
    m_TextureImageView = m_Mother->CreateImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
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

    for (auto shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex;

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
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

    m_Mother->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Vertices.data(), bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    m_Mother->CreateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);

    m_Mother->CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void Model::CreateIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    m_Mother->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Indices.data(), bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    m_Mother->CreateBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);

    m_Mother->CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

UniformBufferObject Model::UpdateMVPMatrices(glm::mat4& viewMatrix) {

    UniformBufferObject ubo{};

    ubo.view = viewMatrix;
    ubo.projection = glm::perspective(glm::radians(60.0f), m_Width / (float)m_Height, 0.1f, 20.0f);
    ubo.projection[1][1] *= -1;

    return ubo;
}

void Model::Cleanup() {
    vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
    vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
    vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
    vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
    vkDestroyImageView(m_Device, m_TextureImageView, nullptr);
    vkDestroyImage(m_Device, m_TextureImage, nullptr);
    vkFreeMemory(m_Device, m_TextureImageMemory, nullptr);
}

VkCommandBuffer* Model::Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo, VkPipelineLayout& pipelineLayout, glm::mat4& viewMatrix) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currTime - startTime).count();

    VkDeviceSize offsets = 0;
    // vkResetCommandBuffer(m_CommandBuffers[index], 0);
    vkBeginCommandBuffer(m_CommandBuffers[index], beginInfo);
    vkCmdBindPipeline(m_CommandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, m_Mother->m_GraphicsPipeline);
    vkCmdBindVertexBuffers(m_CommandBuffers[index], 0, 1, &m_VertexBuffer, &offsets);
    vkCmdBindIndexBuffer(m_CommandBuffers[index], m_IndexBuffer, offsets, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(m_CommandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &m_DescriptorSets[index], 0, nullptr);
    for (auto& instance : m_Instances) {
        UniformBufferObject ubo = UpdateMVPMatrices(viewMatrix);
        ubo.model = instance.GetModelMatrix(time);
        vkCmdPushConstants(m_CommandBuffers[index], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ubo), &ubo);
        vkCmdDrawIndexed(m_CommandBuffers[index], m_Indices.size(), 1, 0, 0, 0);
    }
    vkEndCommandBuffer(m_CommandBuffers[index]);
    return &m_CommandBuffers[index];
}
