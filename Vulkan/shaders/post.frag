#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D txt;

void main()
{
  float gamma = 1.8 / 2.2;
  fragColor = pow(texture(txt, inUV).rgba, vec4(gamma));
}