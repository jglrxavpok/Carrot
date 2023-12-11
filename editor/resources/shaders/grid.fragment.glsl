#include "../../../engine/resources/shaders/includes/gbuffer.glsl"
#include "../../../engine/resources/shaders/includes/gbuffer_output.glsl"

#include "../../../engine/resources/shaders/includes/camera.glsl"
DEFINE_CAMERA_SET(0)

#include "grid.glsl"
#include "../../../engine/resources/shaders/includes/math.glsl"

layout(location = 0) in vec2 inVertexPosition;
layout(location = 1) in vec3 inViewPosition;
layout(location = 2) in flat mat4 inModelView;

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

    GBuffer gbuffer = initGBuffer(inModelView);

    gbuffer.albedo = grid.color;
    gbuffer.viewPosition = inViewPosition;
    outputGBuffer(gbuffer, inModelView);
}