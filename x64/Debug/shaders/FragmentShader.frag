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

vec4 ownColor = vec4(0.75, 0.75, 0.75, 1.0);

vec4 cookTorrance(vec4 lightPosition, vec3 E, float metalicness) {
    vec3 N = fragNormal;
    vec3 L = normalize(vec3(lightPosition) - fragCoord);
    vec3 H = normalize(L + E);
    float e = 2.71828, pi = 3.1415926535897932384626433832795, m = 0.1;
    float cosAlpha = dot(N, H);
    float cosTheta = dot(E, H);
    float ior = 1.13511 ; // indice of refraction for silver

    // D
    float eExponent = pow(tan(acos(cosAlpha)) / m, 2) * -1;
    float D = (1.0 / (pow(m, 2) * pow(cosAlpha, 4))) * pow(e, eExponent);

    // G
    float G = min(min(1.0, (2.0 * cosAlpha * dot(N, E) / cosTheta)), (2.0 * cosAlpha * dot(N, L) / cosTheta));
    G = clamp(G, 0, 1);

    // F using Schlick approx
    vec4 F0 = vec4(pow(ior - 1, 2) / pow(ior + 1, 2));
    //F0 = mix(F0, ownColor, metalicness);
    vec4 fresnel = F0 + (1.0 - F0) * pow(1.0 - clamp(cosTheta, 0, 1), 5);

    return (fresnel / pi) * (D * G / (dot(N, L) * dot(N, E)));
}

vec4 lightContribution(vec4 lightPosition, vec3 E, vec4 lightColor) {
    float specularShare = 0.8;
    float blinnContribution = 0.0;
    float specularPower = 32;

    float distanceFromLight = length(vec3(lightPosition) - fragCoord);
    vec3 L = normalize(vec3(lightPosition) - fragCoord);

    // lambertian factor
    float lambertian = clamp(dot(fragNormal, L), 0.0, 1.0);

    //// phong specular
    //vec3 R = reflect(-L, fragNormal);
    //float phongSpecular = pow(max(dot(R, E), 0.0), specularPower);
    //
    //// blinn-phong
    //vec3 H = normalize(L + E);
    //float blinnSpecular = pow(max(dot(fragNormal, H), 0.0), 4 * specularPower);
    //
    //float specular = blinnSpecular * blinnContribution + phongSpecular * (1 - blinnContribution);
    vec4 specular = cookTorrance(lightPosition, E, specularShare);

    vec4 pd = ownColor * lambertian * lightColor;

    return (specular * lightColor) * specularShare + pd * (1 - specularShare);
}

void main() {
    // constants
    vec4 ambientColor = vec4(0.01, 0.01, 0.01, 1.0);

    if (length(vec3(lights.red) - fragCoord) < 0.1) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else if (length(vec3(lights.green) - fragCoord) < 0.1) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        // common
        vec3 E = normalize(vec3(lights.camera) - fragCoord);

        outColor = lightContribution(lights.red, E, vec4(1.0, 0.0, 0.0, 1.0) * 0.3);
        outColor += lightContribution(lights.green, E, vec4(1.0, 1.0, 1.0, 1.0) * 0.3);
        outColor += ambientColor * ownColor;
        outColor = min(outColor, vec4(1.0));
    }
}
