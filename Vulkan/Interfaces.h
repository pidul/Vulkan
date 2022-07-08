#pragma once
#include "CommonHeaders.h"
#include "Vertex.h"

class DrawableInterface {
public:
    virtual VkCommandBuffer* Draw(uint32_t index, VkCommandBufferBeginInfo* beginInfo, glm::mat4& viewMatrix, LightsPositions lp) = 0;
    virtual void UpdateLightPosition(uint32_t index, glm::vec4 position) = 0;
    virtual void UpdateWindowSize() = 0;
    virtual void Cleanup() = 0;
};
