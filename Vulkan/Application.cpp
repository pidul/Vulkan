#include "Application.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

void Application::Run() {
    InitWindow();
    InitVulkan();
    m_Lights.red = glm::vec4( std::cos(M_PI / 180 * -40), std::sin(M_PI / 180 * -40), 0.8f, 1.0f);
    m_Lights.green = glm::vec4(std::cos(M_PI / 180 * 220), std::sin(M_PI / 180 * 220), 0.8f, 1.0f);
    m_Lights.blue = glm::vec4(std::cos(/*M_PI / 180 * 240 */0), std::sin(/*M_PI / 180 * 240*/ 0), 2.0f, 1.0f);

    //Skybox skybox;
    //m_Models.push_back(&skybox);

#ifdef TESTING
    RaytracedModel sphere({ "models/sphere.obj" });
    sphere.AddInstance(glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(1.0f, 0.0f, 0.0f));
    sphere.PrepareForRayTracing();
    m_Models.push_back(&sphere);
#else
    RaytracedModel skull({ "models/skull.obj", "models/jaw.obj", "models/teethUpper.obj", "models/teethLower.obj" });
    skull.AddInstance(glm::vec3(0.0f, 10.0f, 1.0f), glm::vec3(0.3f, 0.3f, 0.3f), glm::vec3(1.0f, 0.0f, 0.0f));
    skull.PrepareForRayTracing();
    m_Models.push_back(&skull);
#endif

    m_Camera.m_Position = glm::vec3(0.0f, 0.0f, 13.0f);
    m_Camera.m_LookAt = glm::vec3(0.0f, 0.0, 2.0f);
    MainLoop();
    Cleanup();
}

void Application::RecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }

    m_VkFactory->RecreateSwapChain();
    for (auto& model : m_Models) {
        model->UpdateWindowSize();
    }
}

void Application::InitWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_IsFullscreen = false;
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    m_Window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Vulkan master thesis", nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetKeyCallback(m_Window, KeyboardInputCallback);
    glfwSetCursorPosCallback(m_Window, MouseInputCallback);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
}

void Application::InitVulkan() {
    m_VkFactory = VulkanFactory::GetInstance();
    m_VkFactory->InitVulkan(m_Window);
}

void Application::RecordCommandBuffers(uint32_t index) {
    VkCommandBufferBeginInfo primaryCmdBuffbeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,                // sType
        nullptr,                                                    // pNext
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,                // flags
        nullptr                                                     // pInheritanceInfo
    };

    vkResetCommandBuffer(m_VkFactory->GetCommandBuffer(index), 0);
    if (vkBeginCommandBuffer(m_VkFactory->GetCommandBuffer(index), &primaryCmdBuffbeginInfo) != VK_SUCCESS) {
        throw std::runtime_error("cannot begin command buffer");
    }
    vkCmdResetQueryPool(m_VkFactory->GetCommandBuffer(index), m_VkFactory->GetQueryPool(), index * 2, 2);
    if (!m_rtEnabled) {
        //VkCommandBufferInheritanceInfo inheritanceInfo = {
        //    VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,          // sType
        //    nullptr,                                                    // pNext
        //    m_VkFactory->GetRenderPass(),                               // renderPass
        //    0,                                                          // subpass
        //    m_VkFactory->GetFramebuffer(index),                         // framebuffer
        //    VK_FALSE,                                                   // occlusionQueryEnable
        //    0,                                                          // queryFlags
        //    0,                                                          // pipelineStatistics
        //};

        //VkCommandBufferBeginInfo secondaryCmdBuffBeginInfo = {
        //    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,                // sType
        //    nullptr,                                                    // pNext
        //    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,           // flags
        //    &inheritanceInfo                                            // pInheritanceInfo
        //};

        //std::array<VkClearValue, 2> clearColors = {};
        //clearColors[0] = { 0.0f, 0.0f, 0.0f, 1.0f };
        //// clearColors[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
        //clearColors[1] = { 1.0f, 0 };

        //VkRenderPassBeginInfo renderPassBeginInfo = {
        //    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,               // sType
        //    nullptr,                                                // pNext
        //    m_VkFactory->GetRenderPass(),                                           // renderPass
        //    m_VkFactory->GetFramebuffer(index),                         // framebuffer
        //    { { 0, 0 }, m_VkFactory->GetExtent() },                        // renderArea
        //    static_cast<uint32_t>(clearColors.size()),              // clearValueCount
        //    clearColors.data()                                      // pClearValues
        //};

        //static auto startTime = std::chrono::high_resolution_clock::now();
        //auto currTime = std::chrono::high_resolution_clock::now();
        //float time = std::chrono::duration<float, std::chrono::seconds::period>(currTime - startTime).count();
        //glm::mat4 viewMatrix = m_Camera.GetViewMatrix();

        //glm::mat4 lightsTranslation = glm::translate(glm::mat4(1.0f), glm::vec3(m_LightsMoveX, m_LightsMoveY, 0.0f));
        //glm::mat4 lightsRotation = glm::rotate(lightsTranslation, time * 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        //LightsPositions lp{
        //    /*lightsRotation **/ m_Lights.red,
        //    /*lightsRotation **/ m_Lights.green,
        //    glm::vec4(m_Camera.m_Position, 1.0f)
        //};
        //m_Models[1]->UpdateLightPosition(0, lp.red);
        //m_Models[1]->UpdateLightPosition(1, lp.green);

        //vkCmdBeginRenderPass(m_VkFactory->GetCommandBuffer(index), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        //for (auto &model : m_Models) {
        //    VkCommandBuffer *secondaryCmdBuffer = model->Draw(index, &secondaryCmdBuffBeginInfo, viewMatrix, lp);

        //    vkCmdExecuteCommands(m_VkFactory->GetCommandBuffer(index), 1, secondaryCmdBuffer);
        //}

        //vkCmdEndRenderPass(m_VkFactory->GetCommandBuffer(index));
    } else {
        vkCmdWriteTimestamp(m_VkFactory->GetCommandBuffer(index), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_VkFactory->GetQueryPool(), index * 2);
        m_Models[0]->Raytrace(m_VkFactory->GetCommandBuffer(index), m_Camera.GetViewMatrix(), index, m_UseLtc);

        // postprocess

        std::array<VkClearValue, 2> clearColors = {};
        clearColors[0] = { 0.0f, 0.0f, 0.0f, 1.0f };
        // clearColors[0] = { 0.1f, 0.1f, 0.1f, 1.0f };
        clearColors[1] = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,               // sType
            nullptr,                                                // pNext
            m_VkFactory->GetRenderPass(),                           // renderPass
            m_VkFactory->GetFramebuffer(index),                     // framebuffer
            { { 0, 0 }, m_VkFactory->GetExtent() },                 // renderArea
            static_cast<uint32_t>(clearColors.size()),              // clearValueCount
            clearColors.data()                                      // pClearValues
        };

        vkCmdBeginRenderPass(m_VkFactory->GetCommandBuffer(index), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_Models[0]->Postprocess(m_VkFactory->GetCommandBuffer(index), index);
        vkCmdEndRenderPass(m_VkFactory->GetCommandBuffer(index));
        vkCmdWriteTimestamp(m_VkFactory->GetCommandBuffer(index), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkFactory->GetQueryPool(), index * 2 + 1);
    }

    if (vkEndCommandBuffer(m_VkFactory->GetCommandBuffer(index)) != VK_SUCCESS) {
        throw std::runtime_error("cannot end command buffer");
    }
}

void Application::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void Application::KeyboardInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
        glfwSetWindowShouldClose(window, 1);
    }
    else if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        if (app->m_IsFullscreen) {
            const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            glfwSetWindowMonitor(app->m_Window, nullptr, 0, 0, 800, 600, GLFW_DONT_CARE);
            app->m_IsFullscreen = false;
        }
        else {
            const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            glfwSetWindowMonitor(app->m_Window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
            app->m_IsFullscreen = true;

        }
    }
    int state = glfwGetKey(window, GLFW_KEY_W);
    if (state == GLFW_PRESS) {
        app->m_Camera.MovePosition(MoveDirection::up);
    }
    state = glfwGetKey(window, GLFW_KEY_S);
    if (state == GLFW_PRESS) {
        app->m_Camera.MovePosition(MoveDirection::down);
    }
    state = glfwGetKey(window, GLFW_KEY_A);
    if (state == GLFW_PRESS) {
        app->m_Camera.MovePosition(MoveDirection::left);
    }
    state = glfwGetKey(window, GLFW_KEY_D);
    if (state == GLFW_PRESS) {
        app->m_Camera.MovePosition(MoveDirection::right);
    }

    state = glfwGetKey(window, GLFW_KEY_UP);
    if (state == GLFW_PRESS) {
        app->m_Lights.red.z += 0.1f;
        app->m_Lights.green.z += 0.1f;
        app->m_Lights.blue.z += 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_DOWN);
    if (state == GLFW_PRESS) {
        app->m_Lights.red.z -= 0.1f;
        app->m_Lights.green.z -= 0.1f;
        app->m_Lights.blue.z -= 0.1f;
    }
    static float radius = 1.0f;
    bool updateLights = false;
    state = glfwGetKey(window, GLFW_KEY_RIGHT);
    if (state == GLFW_PRESS) {
        updateLights = true;
        radius += 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_LEFT);
    if (state == GLFW_PRESS) {
        updateLights = true;
        radius -= 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_J);
    if (state == GLFW_PRESS) {
        app->m_LightsMoveX-= 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_L);
    if (state == GLFW_PRESS) {
        app->m_LightsMoveX += 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_I);
    if (state == GLFW_PRESS) {
        app->m_LightsMoveY += 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_K);
    if (state == GLFW_PRESS) {
        app->m_LightsMoveY -= 0.1f;
    }
    state = glfwGetKey(window, GLFW_KEY_R);
    if (state == GLFW_PRESS) {
        app->m_UseLtc = !app->m_UseLtc;
    }
    if (updateLights) {
        app->m_Lights.red =   glm::vec4(radius * std::cos(0), radius * std::sin(0), app->m_Lights.red.z, 1.0f);
        app->m_Lights.green = glm::vec4(radius * std::cos(M_PI / 180 * 120), radius * std::sin(M_PI / 180 * 120), app->m_Lights.green.z, 1.0f);
        app->m_Lights.blue =  glm::vec4(radius * std::cos(M_PI / 180 * 240), radius * std::sin(M_PI / 180 * 240), app->m_Lights.red.z, 1.0f);
    }
}

void Application::MouseInputCallback(GLFWwindow* window, double xpos, double ypos) {
#define FRAME_CAPTURE
#ifndef FRAME_CAPTURE
    static double prevX = 0.0, prevY = 0.0;
    double dX = xpos - prevX;
    double dY = ypos - prevY;
    prevX = xpos; prevY = ypos;
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    if (dX < 0) {
        app->m_Camera.MoveTarget(MoveDirection::left);
    } else if (dX > 0) {
        app->m_Camera.MoveTarget(MoveDirection::right);
    }
    
    if (dY < 0) {
        app->m_Camera.MoveTarget(MoveDirection::down);
    }
    else if (dY > 0) {
        app->m_Camera.MoveTarget(MoveDirection::up);
    }
#endif
}

void Application::MainLoop() {
    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();
        DrawFrame();
    }
}

void Application::DrawFrame() {
    VkResult result;
    uint32_t imageIndex;
    static uint32_t semaphoreIndex = 0;
    semaphoreIndex = ++semaphoreIndex % m_VkFactory->GetSwapchainImages().size();
    VkSemaphore& imageReadySemaphore = m_VkFactory->GetImageReadySemaphore(semaphoreIndex);
    result = vkAcquireNextImageKHR(m_VkFactory->GetDevice(), m_VkFactory->GetSwapchain(), UINT64_MAX, imageReadySemaphore, VK_NULL_HANDLE, &imageIndex);
    
    static uint32_t frameCount = 0;
    //std::cout << "frame: " << frameCount++ << "\n";

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swapchain image");
    }

    VkSemaphore waitSemaphores[] = { imageReadySemaphore };
    VkSemaphore signalSemaphores[] = { m_VkFactory->GetRenderFinishedSemaphore(semaphoreIndex) };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    if (vkWaitForFences(m_VkFactory->GetDevice(), 1, &m_VkFactory->GetCmdBuffFence(imageIndex), VK_TRUE, 100000000) == VK_TIMEOUT) {
        std::cout << "vkWaitForFences timeouted!\n";
    }

    if (frameCount++ > 2) {
        m_VkFactory->FetchRenderTimeResults(imageIndex);
    }

    RecordCommandBuffers(imageIndex);
    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,                              // sType
        nullptr,                                                    // pNext
        1,                                                          // waitSemaphoreCount
        waitSemaphores,                                             // pWaitSemaphores
        waitStages,                                                 // pWaitDstStageMask
        1,                                                          // commandBufferCount
        &m_VkFactory->GetCommandBuffer(imageIndex),                              // pCommandBuffers
        1,                                                          // signalSemaphoreCount
        signalSemaphores                                            // pSignalSemaphores
    };
    vkResetFences(m_VkFactory->GetDevice(), 1, &m_VkFactory->GetCmdBuffFence(imageIndex));
    if (vkQueueSubmit(m_VkFactory->GetQueue(), 1, &submitInfo, m_VkFactory->GetCmdBuffFence(imageIndex)) != VK_SUCCESS) {
        throw std::runtime_error("queue submit failed");
    }

    VkPresentInfoKHR presentInfo = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,                         // sType
        nullptr,                                                    // pNext
        1,                                                          // waitSemaphoreCount
        signalSemaphores,                                           // pWaitSemaphores
        1,                                                          // swapchainCount
        &m_VkFactory->GetSwapchain(),                               // pSwapchains
        &imageIndex,                                                // pImageIndices
        nullptr                                                     // pResults
    };

    result = vkQueuePresentKHR(m_VkFactory->GetQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        RecreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present image");
    }
}


void Application::Cleanup() {
    m_VkFactory->CleanupSwapChain();

    for (auto& model : m_Models) {
        model->Cleanup();
    }

    m_VkFactory->Cleanup();

    glfwDestroyWindow(m_Window);
    glfwTerminate();
}
