#include <includes/number-print.glsl>
#include "template.fragment.glsl"

vec4 debugColor(uint triangleIndex, uint instanceIndex, uint clusterID) {
    uvec2 visibilityBufferImageSize = imageSize(inputImage);
    vec2 startPosition = floor(uv * 100) / 100.0f;
    vec2 fontSize = vec2(4, 5) / visibilityBufferImageSize;
    uint lod = clusters[clusterID].lod;

    float digit = PrintValue(uv, startPosition, fontSize, lod, 2, 0);
    const vec4 digitColor = vec4(0, 0, 0, 1);
    const vec4 objectColor = triangleIndexToFloat(lod+1);
    return digit * digitColor + (1 - digit) * objectColor;
}