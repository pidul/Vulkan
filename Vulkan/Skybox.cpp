#include "Skybox.h"

#include <stb_image.h>
#include <tiny_obj_loader.h>

Skybox::Skybox() :
    m_VkFactory(VulkanFactory::GetInstance()), m_VertexBuffer(VK_NULL_HANDLE), m_IndexBuffer(VK_NULL_HANDLE),
    m_VertexBufferMemory(VK_NULL_HANDLE), m_IndexBufferMemory(VK_NULL_HANDLE),
    m_TextureImage(VK_NULL_HANDLE), m_TextureImageMemory(VK_NULL_HANDLE), m_TextureImageView(VK_NULL_HANDLE) {
    LoadModel();
    CreateTextureImage({"textures/posx.jpg", "textures/negx.jpg", "textures/posy.jpg" , "textures/negy.jpg" , "textures/posz.jpg" , "textures/negz.jpg" });
    CreateTextureImageView();
    CreateVertexBuffer();
    CreateIndexBuffer();
    UpdateWindowSize();
}

void Skybox::Cleanup() {
    vkDestroyBuffer(m_VkFactory->GetDevice(), m_VertexBuffer, nullptr);
    vkDestroyBuffer(m_VkFactory->GetDevice(), m_IndexBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_VertexBufferMemory, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_IndexBufferMemory, nullptr);
    vkDestroyImageView(m_VkFactory->GetDevice(), m_TextureImageView, nullptr);
    vkDestroyImage(m_VkFactory->GetDevice(), m_TextureImage, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_TextureImageMemory, nullptr);
}

void Skybox::UpdateWindowSize() {
    m_Width = m_VkFactory->GetExtent().width;
    m_Height = m_VkFactory->GetExtent().height;
    m_VkFactory->CreateDescriptorSets(m_DescriptorSets, m_TextureImageView);
    m_VkFactory->AllocateSecondaryCommandBuffer(m_CommandBuffers);
}

void Skybox::CreateTextureImage(std::vector<std::string> textures) {
    std::vector<stbi_uc*> vecPixels;
    int texWidth = 0, texHeight = 0, texChannels = 0;
    assert(textures.size() == 6);
    for (const std::string& texPath : textures) {
        stbi_uc* pixels = stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("cannot load texture");
        }
        vecPixels.push_back(pixels);
    }
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    uint32_t faceSize = (VkDeviceSize)texWidth * texHeight * 4;

    VkDeviceSize imageSize = 6 * faceSize;

    m_VkFactory->CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    for (uint32_t i = 0; i < 6; ++i) {
        memcpy((void*)((char*)data + i * faceSize), vecPixels[i], faceSize);
    }
    vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory);

    for (stbi_uc*& pixels : vecPixels) {
        stbi_image_free(pixels);
    }

    m_VkFactory->CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_TextureImage, m_TextureImageMemory, 6);

    m_VkFactory->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_VkFactory->CopyBufferToImage(stagingBuffer, m_TextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    m_VkFactory->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory, nullptr);
}

void Skybox::CreateTextureImageView() {
    m_TextureImageView = m_VkFactory->CreateImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Skybox::LoadModel() {
    std::string modelPath = "models/cube.obj";
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

void Skybox::CreateVertexBuffer() {
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

void Skybox::CreateIndexBuffer() {
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
