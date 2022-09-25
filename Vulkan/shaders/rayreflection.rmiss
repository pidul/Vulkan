#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT hitpayload{ vec3 hitValue; float len; } prd;

layout(set = 2, binding = 0) uniform samplerCube Cubemap;

void main() {
  prd.hitValue = textureLod(Cubemap, gl_WorldRayDirectionEXT.xzy, 0/*log(prd.len)*/).xyz;
}
