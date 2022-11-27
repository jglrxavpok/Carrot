#include "../../../engine/resources/shaders/includes/gbuffer_output.glsl"

#include "../../../engine/resources/shaders/includes/camera.glsl"
DEFINE_CAMERA_SET(0)

#include "grid.glsl"
#include "../../../engine/resources/shaders/includes/math.glsl"

layout(location = 0) in vec2 inVertexPosition;
layout(location = 1) in vec3 inViewPosition;

void main() {
    vec2 positionInCell = mod(inVertexPosition, grid.cellSize);
    if(positionInCell.x < 0) {
        positionInCell.x = grid.size + positionInCell.x;
    }
    if(positionInCell.y < 0) {
        positionInCell.y = grid.size + positionInCell.y;
    }

    vec2 thickness = vec2(grid.lineWidth);
    vec2 borderDistance = (1-step(positionInCell, thickness)) * step(positionInCell, vec2(grid.cellSize)-thickness);

    if(borderDistance.x > thickness.x && borderDistance.y > thickness.y) {
        discard;
    }

    outColor = grid.color;
    outViewPosition = vec4(inViewPosition, 1.0);
    outNormal = vec4(0, 0, 1, 0);
    outTangent = vec4(1, 0, 0, 0);
    intProperty = 0;
    entityID = uvec4(0);
    metallicRoughness = vec4(0.0);
    emissive = vec4(0.0);
}