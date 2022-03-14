#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <array>
#include <optional>
#include <chrono>
#include <unordered_map>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtx/normal.hpp>

#include <iostream>
#include <set>
#include <mutex>

typedef std::optional<uint32_t> QueueFamilyIndices;

const uint32_t DEFAULT_WIDTH = 800;
const uint32_t DEFAULT_HEIGHT = 600;

const uint32_t MAX_OBJECTS_ON_SCENE = 5;
