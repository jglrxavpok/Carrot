#include "template.fragment.glsl"

vec4 debugColor(uint triangleIndex, uint instanceIndex, uint clusterID) {
    return triangleIndexToFloat(clusterID);
}