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
    layout(offset = 224) vec4 camera;
} lights;

layout(location = 0) out vec4 outColor;

vec4 lightContribution(vec4 lightPosition, vec3 E, vec4 lightColor) {
    vec4 ownColor = vec4(0.8, 0.8, 0.8, 1.0);
    float specularShare = 0.95;
    float blinnContribution = 1.0;
    float specularPower = 16;

    float distanceFromLight = length(vec3(lightPosition) - fragCoord);
    vec3 L = normalize(vec3(lightPosition) - fragCoord);

    // lambertian factor
    float lambertian = clamp(dot(fragNormal, L), 0.0, 1.0);

    // phong specular
    vec3 R = reflect(-L, fragNormal);
    float phongSpecular = pow(max(dot(R, E), 0.0), specularPower);

    // blinn-phong
    vec3 H = normalize(L + E);
    float blinnSpecular = pow(max(dot(fragNormal, H), 0.0), 4 * specularPower);

    float specular = blinnSpecular * blinnContribution + phongSpecular * (1 - blinnContribution);

    vec4 pd = ownColor * lambertian  * lightColor;


    return (specular * lightColor) * specularShare + pd * (1 - specularShare);
}

void main() {
    // constants
    vec4 ownColor = vec4(0.8, 0.8, 0.8, 1.0);
    vec4 ambientColor = vec4(0.01, 0.01, 0.01, 1.0);

    if (length(vec3(lights.red) - fragCoord) < 0.1) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else if (length(vec3(lights.green) - fragCoord) < 0.1) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        // common
        vec3 E = normalize(vec3(lights.camera) - fragCoord);

        outColor = lightContribution(lights.red, E, vec4(1.0, 0.0, 0.0, 1.0) * 0.2);
        outColor += lightContribution(lights.green, E, vec4(1.0, 1.0, 1.0, 1.0) * 0.2);
        outColor += ambientColor * ownColor;
        outColor = min(outColor, vec4(1.0));
    }

}
