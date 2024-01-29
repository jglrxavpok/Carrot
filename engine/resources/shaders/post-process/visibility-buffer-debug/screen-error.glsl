#include <includes/number-print.glsl>
#include "template.fragment.glsl"

float projectError(uint instanceID, uint clusterID, float testScreenWidth, float testScreenHeight) {
    const float cotHalfFov = 1.0f / tan(3.1415f/2.0f / 2.0f);
    uint modelDataIndex = instances[instanceID].instanceDataIndex;

    // replicates what is done by ClusterManager
#define cluster (clusters[clusterID])
    const mat4 completeTransform = cbo.view * modelData[modelDataIndex].transform * cluster.transform;
    const vec3 sphereCenter = (completeTransform * vec4(cluster.boundingSphere.xyz, 1.0)).xyz;
    float sphereRadius = cluster.error;

    sphereRadius = length((completeTransform * vec4(sphereRadius, 0,0,0)).xyz);

    if(isinf(sphereRadius)) {
        return sphereRadius;
    }
    const float d2 = dot(sphereCenter, sphereCenter);
    const float r = sphereRadius;
    //return max(r * testScreenWidth / 2.0f, r * testScreenHeight / 2.0f);
    return testScreenHeight * cotHalfFov * r / sqrt(d2 - r*r);
    //return cluster.error;
}

vec4 debugColor(uint triangleIndex, uint instanceIndex, uint clusterID) {
    uvec2 visibilityBufferImageSize = imageSize(inputImage);
    vec2 valueCount = vec2(25, 40);
    vec2 startPosition = floor(uv * valueCount) / valueCount;
    vec2 fontSize = vec2(4, 5) / visibilityBufferImageSize;

    float valueToPrint = projectError(instanceIndex, clusterID, visibilityBufferImageSize.x, visibilityBufferImageSize.y);

    uint lod = clusters[clusterID].lod;

    float digit = PrintValue(uv, startPosition, fontSize, valueToPrint, 0, 8);
    const vec4 digitColor = vec4(0, 0, 0, 1);
    const vec4 objectColor = triangleIndexToFloat(lod+1);

    return digit * digitColor + (1 - digit) * objectColor;
}