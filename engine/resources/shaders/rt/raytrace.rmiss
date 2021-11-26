#extension GL_EXT_ray_tracing : require
#include "raytrace.common.glsl"

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main()
{
    payload.hitColor = vec3(0);
}
