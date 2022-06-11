#version 450
layout(location = 0) in vec3 vertTexcoord;

layout(binding = 0) uniform samplerCube Cubemap;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(Cubemap, vertTexcoord);
}
