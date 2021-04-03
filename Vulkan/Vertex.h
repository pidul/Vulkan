#pragma once

#include "CommonHeaders.h"

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

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

