#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform constants {
    layout(offset = 192) vec4 red;
    layout(offset = 208) vec4 green;
    layout(offset = 224) vec4 blue;
} lights;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragTexCoord);
    
    vec4 fogColor = vec4(0.2, 0.2, 0.2, 1.0);
    float fogFactor = 0.0;

    vec4 ambientColor = vec4(1.0, 1.0, 1.0, 1.0);
    float ambientFactor = 0.2;

    vec3 red = normalize(vec3(lights.red) - fragCoord);
    vec3 green = normalize(vec3(lights.green) - fragCoord);
    vec3 blue = normalize(vec3(lights.blue) - fragCoord);

    float redIntensity = dot(fragNormal, red);
    float greenIntensity = dot(fragNormal, green);
    float blueIntensity = dot(fragNormal, blue);

    outColor = outColor * vec4(redIntensity, greenIntensity, blueIntensity, 1.0f) + ambientColor * ambientFactor * outColor;

    outColor = (1 - fogFactor) * outColor + fogFactor * fogColor;
}