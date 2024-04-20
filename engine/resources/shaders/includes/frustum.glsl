#include "math.glsl"

bool frustumCheck(in vec4 planes[6], vec3 worldSpacePosition, float radius) {
    for(int i = 0; i < 6; i++) {
        if(getSignedDistanceToPlane(planes[i].xyz, planes[i].w, worldSpacePosition) < -radius) {
            return false;
        }
    }
    return true;
}