#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    vec3 fogColor = vec3(0.2f, 0.2f, 0.2f);
    float fogFactor = 0.0;
    float ambientFactor = 1.0;
    vec3 ambientLight = vec3(1.0f, 1.0f, 1.0f);
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor * (ambientFactor * ambientLight);
    fragColor = (1 - fogFactor) * fragColor + fogFactor * fogColor;
    fragTexCoord = inTexCoord;
}