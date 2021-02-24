#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "raytrace.common.glsl"

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main()
{
    payload.hitColor = vec3(0.1,0.1,0.1);
}
