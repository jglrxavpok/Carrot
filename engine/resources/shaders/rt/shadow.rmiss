#extension GL_EXT_ray_tracing : require
#include "raytrace.common.glsl"

layout(location = 1) rayPayloadInEXT bool isShadowed;

void main()
{
    isShadowed = false;
}
