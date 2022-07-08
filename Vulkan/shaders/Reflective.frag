#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragCoord;

layout(binding = 0) uniform samplerCube texSampler;

layout(push_constant) uniform constants {
    layout(offset = 192) vec4 red;
    layout(offset = 208) vec4 green;
    layout(offset = 224) vec4 camera;
} lights;

layout( location = 0 ) out vec4 outColor;

void main() {
	vec3 viewVector = normalize(fragCoord - lights.camera.xyz);
	vec3 reflectVector = reflect(viewVector, fragNormal).xzy;
	vec4 reflectColor = texture(texSampler, reflectVector);
	outColor = reflectColor;
}
