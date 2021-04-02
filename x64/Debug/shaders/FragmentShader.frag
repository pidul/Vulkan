#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragTexCoord);
    
    vec4 fogColor = vec4(0.2f, 0.2f, 0.2f, 1.0f);
    float fogFactor = 0.6;

    outColor = (1 - fogFactor) * outColor + fogFactor * fogColor;
}