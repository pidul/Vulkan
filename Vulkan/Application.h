#pragma once

#include "CommonHeaders.h"
#include "Vertex.h"
#include "Model.h"
#include "Camera.h"
#include "VulkanFactory.h"
#include "RaytracedModel.h"

#include "Skybox.h"
#include "ReflectiveModel.h"

class Model;

class Application {
    friend class Model;
    friend class Skybox;

public:
    void Run();

private:
    VulkanFactory* m_VkFactory;

    std::vector<RaytracedModel*> m_Models;
    Camera m_Camera;
    LightsPositions m_Lights;
    float m_LightsMoveX = 0.0f, m_LightsMoveY = 0.0f;
    bool m_UseLtc = true;
    bool m_IsFullscreen;

    bool framebufferResized = false;
    GLFWwindow* m_Window;

    bool m_rtEnabled = true;

    void InitWindow();
    static void FramebufferResizeCallback(GLFWwindow*, int, int);
    static void KeyboardInputCallback(GLFWwindow*, int, int, int, int);
    static void MouseInputCallback(GLFWwindow*, double, double);
    void InitVulkan();
    
    void RecreateSwapChain();
    void RecordCommandBuffers(uint32_t index);
    
    void MainLoop();
    void DrawFrame();
    void Cleanup();
};