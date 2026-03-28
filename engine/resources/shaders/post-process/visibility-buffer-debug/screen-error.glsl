#include <includes/number-print.glsl>
#include "template.fragment.glsl"

float projectError(uint instanceID, uint clusterID, float testScreenWidth, float testScreenHeight) {
    const float cotHalfFov = 1.0f / tan(3.1415f/2.0f / 2.0f);
    uint modelDataIndex = instances[instanceID].instanceDataIndex;
    #define cluster (clusters[clusterID])
    vec3 cameraPos = (cbo.view * vec4(0,0,0,1)).xyz;
    const mat4 completeTransform = modelData[modelDataIndex].instanceData.transform * mat4(cluster.transform);
    return computeClusterScreenError(cameraPos, completeTransform, cluster.boundingSphere, cluster.error) * testScreenHeight;
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