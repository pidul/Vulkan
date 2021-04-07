#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform constants {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragCoord;

void main() {
    vec4 normal = ubo.model * vec4(inNormal, 1.0);
    fragNormal = vec3(normalize(normal));

    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;


    fragCoord = vec3(ubo.model * vec4(inPosition, 1.0));
}
