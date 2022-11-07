#include "RaytracedModel.h"

#include <stb_image.h>
#include <tiny_obj_loader.h>


RaytracedModel::RaytracedModel(std::vector<std::string> modelFilenames) :
    m_VertexBuffer(VK_NULL_HANDLE), m_IndexBuffer(VK_NULL_HANDLE),
    m_VertexBufferMemory(VK_NULL_HANDLE), m_IndexBufferMemory(VK_NULL_HANDLE),
    m_VkFactory(VulkanFactory::GetInstance()) {
    for (const auto &modelFilename : modelFilenames) {
        LoadModel(modelFilename);
    }
    CreateDescriptorSetLayout();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateDescriptorPool();
    //CreateTextureImage({ "textures/posx.jpg", "textures/negx.jpg", "textures/posy.jpg" , "textures/negy.jpg" , "textures/posz.jpg" , "textures/negz.jpg" });
    CreateTextureImage({ "textures/posx2.jpg", "textures/negx2.jpg", "textures/posy2.jpg" , "textures/negy2.jpg" , "textures/posz2.jpg" , "textures/negz2.jpg" });
    //CreateTextureImage({ "textures/black.jpg", "textures/black.jpg", "textures/black.jpg" , "textures/black.jpg" , "textures/white.jpg" , "textures/black.jpg" });
    CreateLTCImage();
    m_VkFactory->CreateTextureSampler(m_TextureSampler);
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (uint32_t i = 0; i < m_LTCImage.size(); ++i) {
        m_VkFactory->CreateTextureSampler(m_LTCSampler[i]);
        imageInfos.push_back({
                m_LTCSampler[i],                                    // sampler
                m_LTCImageView[i],                                  // imageView
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL            // imageLayout
            });
    }
    m_VkFactory->CreateMultipleTextureDescriptorSets(m_LTCDescriptorSets, imageInfos, m_LTCDescriptorSetLayout, m_LTCDescriptorPool);
    m_VkFactory->CreateTextureDescriptorSets(m_SkyboxDescriptorSets, m_TextureImageView, m_TextureSampler, m_SkyboxDescriptorSetLayout, m_SkyboxDescriptorPool);
    CreateUniformBuffer();
    m_OffscreenRenderTargets.resize(m_VkFactory->GetSwapchainImages().size());
    for (uint32_t i = 0; i < m_VkFactory->GetSwapchainImages().size(); ++i) {
        m_OffscreenRenderTargets[i] = m_VkFactory->CreateOffscreenRenderer();
    }
    CreatePostPipeline();

    std::vector<VkDescriptorSetLayout> layouts(m_VkFactory->GetSwapchainImages().size(), m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,             // sType
        nullptr,                                                    // pNext
        m_DescriptorPool,                                           // descriptorPool
        static_cast<uint32_t>(layouts.size()),                      // descriptorSetCount
        layouts.data()                                              // pSetLayouts
    };
    m_DescriptorSets.resize(layouts.size());

    if (vkAllocateDescriptorSets(m_VkFactory->GetDevice(), &allocateInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("cannot allocate descriptor sets");
    }
    m_Width = m_VkFactory->GetExtent().width;
    m_Height = m_VkFactory->GetExtent().height;

    m_BufferAddresses.push_back({ m_VkFactory->GetBufferAddress(m_VertexBuffer), m_VkFactory->GetBufferAddress(m_IndexBuffer) });

    m_VkFactory->CreateBuffer(m_BufferAddresses.size() * sizeof(BufferAddresses), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_AddressesStorageBuffer, m_AddressesStorageBufferMemory);

    VkCommandBuffer cmdBuff = m_VkFactory->BeginSingleTimeCommands();
    vkCmdUpdateBuffer(cmdBuff, m_AddressesStorageBuffer, 0, m_BufferAddresses.size() * sizeof(BufferAddresses), m_BufferAddresses.data());
    m_VkFactory->EndSingleTimeCommands(cmdBuff);
}

void RaytracedModel::UpdateWindowSize() {
    m_Width = m_VkFactory->GetExtent().width;
    m_Height = m_VkFactory->GetExtent().height;
    // m_VkFactory->UpdateRtDescriptorSets(m_RtDescriptorSets);
}

void RaytracedModel::LoadModel(std::string modelPath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warning, error;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, modelPath.c_str())) {
        throw std::runtime_error(warning + error);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (auto &shape : shapes) {
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.texcoord_index == -1) {
                vertex.texCoord = { 0.0, 0.0 };
            } else {
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


void RaytracedModel::CreateVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void *data;
    vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Vertices.data(), bufferSize);
    vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory);

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);

    m_VkFactory->CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory, nullptr);
}

void RaytracedModel::CreateIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void *data;
    vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Indices.data(), bufferSize);
    vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory);

    m_VkFactory->CreateBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);

    m_VkFactory->CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory, nullptr);
}

void RaytracedModel::Cleanup() {
    vkDestroyPipelineLayout(m_VkFactory->GetDevice(), m_RtPipelineLayout, nullptr);
    vkDestroyPipeline(m_VkFactory->GetDevice(), m_RtPipeline, nullptr);
    vkDestroyDescriptorPool(m_VkFactory->GetDevice(), m_DescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_VkFactory->GetDevice(), m_DescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_VkFactory->GetDevice(), m_RtDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_VkFactory->GetDevice(), m_RtDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_VkFactory->GetDevice(), m_SkyboxDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_VkFactory->GetDevice(), m_SkyboxDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_VkFactory->GetDevice(), m_LTCDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_VkFactory->GetDevice(), m_LTCDescriptorSetLayout, nullptr);
    vkDestroyBuffer(m_VkFactory->GetDevice(), m_VertexBuffer, nullptr);
    vkDestroyBuffer(m_VkFactory->GetDevice(), m_IndexBuffer, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_VertexBufferMemory, nullptr);
    vkFreeMemory(m_VkFactory->GetDevice(), m_IndexBufferMemory, nullptr);
}

void RaytracedModel::PrepareForRayTracing() {
    m_Blas = m_VkFactory->CreateBLAS(m_VertexBuffer, m_IndexBuffer, (uint32_t)(m_Vertices.size()), (uint32_t)(m_Indices.size() / 3));
    VkTransformMatrixKHR matrix;
    glm::mat4 transformMatrix = m_Instances[0].GetModelMatrix(0.0f);
    memcpy(&matrix, &transformMatrix, sizeof(VkTransformMatrixKHR));
    m_tlasInstance = {
        matrix,                                                     // transform
        0,                                                          // instanceCustomIndex
        0xFF,                                                       // mask
        0,                                                          // instanceShaderBindingTableRecordOffset
        VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,  // flags
        m_VkFactory->GetAccelerationStructureAddress(m_Blas.as)     // accelerationStructureReference
    };
    m_VkFactory->CreateTLAS(m_Tlas, m_tlasInstance, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR, (uint32_t)(m_Vertices.size()), false);

    std::vector<VkDescriptorPoolSize> descrPoolSize = {
        {
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,          // type
            static_cast<uint32_t>(
                m_VkFactory->GetSwapchainImages().size())           // descriptorCount
        },
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                       // type
            static_cast<uint32_t>(
                m_VkFactory->GetSwapchainImages().size())           // descriptorCount
        }
    };
    m_VkFactory->CreateDescriptorPool(descrPoolSize, m_RtDescriptorPool);

    std::vector<VkImageView> imageViews{};
    for (OffscreenRender &off : m_OffscreenRenderTargets) {
        imageViews.push_back(off.targetImageView);
    }

    m_VkFactory->CreateRtDescriptorSets(m_Tlas, m_RtDescriptorSetLayout, m_RtDescriptorPool, m_RtDescriptorSets, imageViews);
    CreateRtPipeline();
    m_VkFactory->CreateShaderBindingTable(m_RtPipeline, m_RgenRegion, m_MissRegion, m_HitRegion, m_CallRegion, m_RtSBTBuffer, m_RtSBTBufferMemory);
}

void RaytracedModel::Raytrace(VkCommandBuffer cmdBuff, glm::mat4 viewMatrix, float time, uint32_t index) {
    VkTransformMatrixKHR matrix;
    glm::mat4 transformMatrix = m_Instances[0].GetModelMatrix(time);
    memcpy(&matrix, &transformMatrix, sizeof(VkTransformMatrixKHR));
    m_tlasInstance.transform = matrix;
    m_VkFactory->CreateTLAS(m_Tlas, m_tlasInstance, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR, (uint32_t)(m_Vertices.size()), true);


    RtUniformBufferObject ubo{};
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), m_Width / (float)m_Height, 0.1f, 200.0f);
    proj[1][1] *= -1;
    ubo.viewProj = viewMatrix * proj;
    ubo.viewInverse = glm::inverse(viewMatrix);
    ubo.projInverse = glm::inverse(proj);

    UpdateUniformBuffer(cmdBuff, ubo);

    VkDescriptorBufferInfo bufferInfo{
        m_UniformBuffer,                                            // buffer
        0,                                                          // offset
        VK_WHOLE_SIZE                                               // range
    };
    VkDescriptorBufferInfo storageBufferInfo{
        m_AddressesStorageBuffer,                                   // buffer
        0,                                                          // offset
        VK_WHOLE_SIZE                                               // range
    };
    std::array<VkWriteDescriptorSet, 2> writeDescriptorSet{};
    writeDescriptorSet[0] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 // sType
        nullptr,                                                // pNext
        m_DescriptorSets[index],                                // dstSet
        0,                                                      // dstBinding
        0,                                                      // dstArrayElement
        1,                                                      // descriptorCount
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                      // descriptorType
        nullptr,                                                // pImageInfo
        &bufferInfo,                                            // pBufferInfo
        nullptr                                                 // pTexelBufferView
    };
    writeDescriptorSet[1] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,                 // sType
        nullptr,                                                // pNext
        m_DescriptorSets[index],                                // dstSet
        1,                                                      // dstBinding
        0,                                                      // dstArrayElement
        1,                                                      // descriptorCount
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                      // descriptorType
        nullptr,                                                // pImageInfo
        &storageBufferInfo,                                     // pBufferInfo
        nullptr                                                 // pTexelBufferView
    };
    vkUpdateDescriptorSets(m_VkFactory->GetDevice(), (uint32_t)(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);

    std::vector<VkDescriptorSet> descSets{ m_RtDescriptorSets[index], m_DescriptorSets[index], m_SkyboxDescriptorSets[index], m_LTCDescriptorSets[index] };
    vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RtPipeline);
    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RtPipelineLayout, 0,
        (uint32_t)(descSets.size()), descSets.data(), 0, nullptr);
    vkCmdPushConstants(cmdBuff, m_RtPipelineLayout,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0, sizeof(RtPushConstants), &m_RtPC);

    m_VkFactory->TraceRays(cmdBuff, &m_RgenRegion, &m_MissRegion, &m_HitRegion, &m_CallRegion);
}

void RaytracedModel::Postprocess(VkCommandBuffer cmdBuff, uint32_t idx) {
    vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PostPipeline);
    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PostPipelineLayout, 0, 1, &m_OffscreenRenderTargets[idx].descriptorSet, 0, nullptr);
    vkCmdDraw(cmdBuff, 6, 1, 0, 0);
}

void RaytracedModel::CreateDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bufferLayoutBinding = {
        {
            0,                                                      // binding
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                      // descriptorType
            1,                                                      // descriptorCount
            VK_SHADER_STAGE_RAYGEN_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                    // stageFlags
            nullptr                                                 // pImmutableSamplers
        },
        {
            1,                                                      // binding
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                      // descriptorType
            1,                                                      // descriptorCount
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                    // stageFlags
            nullptr                                                 // pImmutableSamplers
        }
    };

    m_VkFactory->CreateDescriptorSetLayout(bufferLayoutBinding, m_DescriptorSetLayout);

    std::vector<VkDescriptorSetLayoutBinding> rtLayoutBinding = {
        {
            0,                                                      // binding
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,          // descriptorType
            1,                                                      // descriptorCount
            VK_SHADER_STAGE_RAYGEN_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                    // stageFlags
            nullptr                                                 // pImmutableSamplers
        },
        {
            1,                                                      // binding
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                       // descriptorType
            1,                                                      // descriptorCount
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,                         // stageFlags
            nullptr                                                 // pImmutableSamplers
        }
    };

    m_VkFactory->CreateDescriptorSetLayout(rtLayoutBinding, m_RtDescriptorSetLayout);

    std::vector<VkDescriptorSetLayoutBinding> skyboxLayoutBinding = { {
        0,                                                          // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        1,                                                          // descriptorCount
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                        // stageFlags
        nullptr                                                     // pImmutableSamplers
    } };

    m_VkFactory->CreateDescriptorSetLayout(skyboxLayoutBinding, m_SkyboxDescriptorSetLayout);

    std::vector<VkDescriptorSetLayoutBinding> ltcLayoutBinding = { {
        0,                                                          // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        1,                                                          // descriptorCount
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                        // stageFlags
        nullptr                                                     // pImmutableSamplers
    },
    {
        1,                                                          // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        1,                                                          // descriptorCount
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                        // stageFlags
        nullptr                                                     // pImmutableSamplers
    },
    {
        2,                                                          // binding
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // descriptorType
        1,                                                          // descriptorCount
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                        // stageFlags
        nullptr                                                     // pImmutableSamplers
    } };

    m_VkFactory->CreateDescriptorSetLayout(ltcLayoutBinding, m_LTCDescriptorSetLayout);
}

void RaytracedModel::CreateDescriptorPool() {
    std::vector<VkDescriptorPoolSize> bufferPoolSize = {
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                      // type
            static_cast<uint32_t>(
                    m_VkFactory->GetSwapchainImages().size())       // descriptorCount
        },
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                      // type
            static_cast<uint32_t>(
                    m_VkFactory->GetSwapchainImages().size())       // descriptorCount
        }
    };

    m_VkFactory->CreateDescriptorPool(bufferPoolSize, m_DescriptorPool);

    std::vector<VkDescriptorPoolSize> skyboxPoolSize = { {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // type
        static_cast<uint32_t>(
            m_VkFactory->GetSwapchainImages().size())               // descriptorCount
    } };

    m_VkFactory->CreateDescriptorPool(skyboxPoolSize, m_SkyboxDescriptorPool);

    std::vector<VkDescriptorPoolSize> ltcPoolSize = { {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,                  // type
        static_cast<uint32_t>(
            m_VkFactory->GetSwapchainImages().size())               // descriptorCount
    } };

    m_VkFactory->CreateDescriptorPool(ltcPoolSize, m_LTCDescriptorPool);
}

void RaytracedModel::CreateRtPipeline() {
    std::vector<VkDescriptorSetLayout> layouts{ m_RtDescriptorSetLayout, m_DescriptorSetLayout, m_SkyboxDescriptorSetLayout, m_LTCDescriptorSetLayout };
    m_VkFactory->CreateRtPipeline(layouts, m_RtPipelineLayout, m_RtPipeline, m_ShaderGroups);
}

void RaytracedModel::CreateUniformBuffer() {
    m_VkFactory->CreateBuffer(sizeof(RtUniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_UniformBuffer, m_UniformBufferMemory);
}

void RaytracedModel::UpdateUniformBuffer(VkCommandBuffer cmdBuff, RtUniformBufferObject& ubo) {
    VkBufferMemoryBarrier beforeBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                    // sType
        nullptr,                                                    // pNext
        VK_ACCESS_SHADER_READ_BIT,                                  // srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,                               // dstAccessMask
        0,                                                          // srcQueueFamilyIndex
        0,                                                          // dstQueueFamilyIndex
        m_UniformBuffer,                                            // buffer
        0,                                                          // offset
        sizeof(RtUniformBufferObject),                              // size
    };
    vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
        nullptr, 1, &beforeBarrier, 0, nullptr);

    vkCmdUpdateBuffer(cmdBuff, m_UniformBuffer, 0, sizeof(RtUniformBufferObject), &ubo);

    VkBufferMemoryBarrier afterBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,                    // sType
        nullptr,                                                    // pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,                               // srcAccessMask
        VK_ACCESS_SHADER_READ_BIT,                                  // dstAccessMask
        0,                                                          // srcQueueFamilyIndex
        0,                                                          // dstQueueFamilyIndex
        m_UniformBuffer,                                            // buffer
        0,                                                          // offset
        sizeof(RtUniformBufferObject),                              // size
    };
    vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
        nullptr, 1, &afterBarrier, 0, nullptr);
}

void RaytracedModel::CreatePostPipeline() {
    VkShaderModule vertexShaderModule, fragmentShaderModule;
    m_VkFactory->CreateShaderModule(vertexShaderModule, "shaders/passthroughVert.spv");
    m_VkFactory->CreateShaderModule(fragmentShaderModule, "shaders/postFrag.spv");

    VkPipelineShaderStageCreateInfo vsStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,        // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_SHADER_STAGE_VERTEX_BIT,                                 // stage
        vertexShaderModule,                                         // module
        "main",                                                     // pName
        nullptr                                                     // pSpecializationInfo
    };

    VkPipelineShaderStageCreateInfo fsStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,        // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        VK_SHADER_STAGE_FRAGMENT_BIT,                               // stage
        fragmentShaderModule,                                       // module
        "main",                                                     // pName
        nullptr                                                     // pSpecializationInfo
    };

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vsStageCreateInfo, fsStageCreateInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // sType
        nullptr,                                                    // pNext
        0,                                                          // flags
        0,                                                          // vertexBindingDescriptionCount
        nullptr,                                                    // pVertexBindingDescriptions
        static_cast<uint32_t>(0),                                   // vertexAttributeDescriptionCount
        nullptr                                                     // pVertexAttributeDescriptions
    };

    std::vector<VkPushConstantRange> pushConstantRanges{};

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { m_OffscreenRenderTargets[0].descriptorSetLayout};

    m_VkFactory->CreateGraphicsPipelineLayout(descriptorSetLayouts, pushConstantRanges, m_PostPipelineLayout);
    m_VkFactory->CreateGraphicsPipeline(shaderStages, vertexInputStateCreateInfo, m_PostPipelineLayout, m_PostPipeline, VK_CULL_MODE_NONE, VK_TRUE);

    vkDestroyShaderModule(m_VkFactory->GetDevice(), vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_VkFactory->GetDevice(), fragmentShaderModule, nullptr);
}

void RaytracedModel::CreateTextureImage(std::vector<std::string> textures) {
    int texWidth = 0, texHeight = 0, texChannels = 0;
    assert(textures.size() == 6);
    stbi_load(textures[0].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    std::vector<VkBuffer> stagingBuffer(6);
    std::vector<VkDeviceMemory> stagingBufferMemory(6);
    uint32_t faceSize = (VkDeviceSize)texWidth * texHeight * 4;
    for (uint32_t i = 0; i < 6; ++i) {
        stbi_uc *pixels = stbi_load(textures[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("cannot load texture");
        }

        m_VkFactory->CreateBuffer(faceSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer[i], stagingBufferMemory[i]);

        void *data;
        vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory[i], 0, faceSize, 0, &data);
        memcpy(data, pixels, faceSize);
        vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory[i]);

        stbi_image_free(pixels);
    }

    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    m_VkFactory->CreateImage(texWidth, texHeight, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_TextureImage, m_TextureImageMemory, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, mipLevels);

    m_VkFactory->TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, mipLevels);
    for (uint32_t i = 0; i < textures.size(); ++i) {
        m_VkFactory->CopyBufferToImage(stagingBuffer[i], m_TextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1, i);
    }
    
    m_VkFactory->GenerateMipMaps(m_TextureImage, texWidth, texHeight, mipLevels, 6);

    for (auto &memory : stagingBufferMemory) {
        vkFreeMemory(m_VkFactory->GetDevice(), memory, nullptr);
    }
    for (auto &buffer : stagingBuffer) {
        vkDestroyBuffer(m_VkFactory->GetDevice(), buffer, nullptr);
    }

    m_TextureImageView = m_VkFactory->CreateImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE, 6, mipLevels);
}

void RaytracedModel::CreateLTCImage() {
#include "LTCanisotropicMatrices.inc"
    std::array<VkBuffer, 3> stagingBuffer{};
    std::array<VkDeviceMemory, 3> stagingBufferMemory{};

    void *data;
    for (uint32_t c = 0; c < stagingBuffer.size(); ++c) {
        m_VkFactory->CreateBuffer(LTCsize / 9 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer[c], stagingBufferMemory[c]);
        vkMapMemory(m_VkFactory->GetDevice(), stagingBufferMemory[c], 0, LTCsize / 9 * 4, 0, &data);
        for (int alpha = 0; alpha < 8; ++alpha) {
            for (int lambda = 0; lambda < 8; ++lambda) {
                for (int theta = 0; theta < 8; ++theta) {
                    for (int phi = 0; phi < 8; ++phi) {
                        static_cast<float*>(data)[0 + 4 * (phi + 8 * (theta + 8 * (lambda + 8 * alpha)))] = anisomats[alpha][lambda][theta][phi][3 * c + 0];
                        static_cast<float*>(data)[1 + 4 * (phi + 8 * (theta + 8 * (lambda + 8 * alpha)))] = anisomats[alpha][lambda][theta][phi][3 * c + 1];
                        static_cast<float*>(data)[2 + 4 * (phi + 8 * (theta + 8 * (lambda + 8 * alpha)))] = anisomats[alpha][lambda][theta][phi][3 * c + 2];
                        static_cast<float*>(data)[3 + 4 * (phi + 8 * (theta + 8 * (lambda + 8 * alpha)))] = 0.0;
                    }
                }
            }
        }
        vkUnmapMemory(m_VkFactory->GetDevice(), stagingBufferMemory[c]);
        m_VkFactory->CreateImage(8, 8, 64, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_LTCImage[c], m_LTCImageMemory[c], 1);
        m_VkFactory->TransitionImageLayout(m_LTCImage[c], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
        m_VkFactory->CopyBufferToImage(stagingBuffer[c], m_LTCImage[c], 8, 8, 64, 0);
        m_VkFactory->TransitionImageLayout(m_LTCImage[c], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

        m_LTCImageView[c] = m_VkFactory->CreateImageView(m_LTCImage[c], VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_3D, 1);
        vkFreeMemory(m_VkFactory->GetDevice(), stagingBufferMemory[c], nullptr);
        vkDestroyBuffer(m_VkFactory->GetDevice(), stagingBuffer[c], nullptr);
    }
}

