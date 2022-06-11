#version 450
layout(location = 0) in vec4 inPos;

layout(push_constant) uniform constants {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) out vec3 vertTexcoord;

void main() {
    vec3 position = mat3(ubo.model * ubo.view) * inPos.xyz;
    gl_Position = (ubo.projection * vec4(position, 0.0)).xyzz;
    vertTexcoord = inPos.xzy;
}
