#version 450
layout(location = 0) in vec4 position;

layout(push_constant) uniform constants {
    mat4 modelView
    mat4 projection;
} ubo;

layout(location = 0) out vec3 vertTexcoord;

void main() {
    vec3 position = mat3(ubo.modelView) * position.xyz;
    gl_Position = (ProjectionMatrix * vec4(postion, 0.0)).xyzz;
    vertTexcoord = position;
}