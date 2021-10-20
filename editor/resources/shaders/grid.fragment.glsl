#version 450
#extension GL_ARB_separate_shader_objects : enable

// TODO: find cleaner way to include:
#include "../../../resources/shaders/includes/gbuffer.glsl"

layout(constant_id = 0) const uint MAX_TEXTURES = 1;
layout(constant_id = 1) const uint MAX_MATERIALS = 1;

struct MaterialData {
    uint textureIndex;
    bool ignoresInstanceColor;
};

layout(set = 0, binding = 0) uniform texture2D textures[MAX_TEXTURES];
layout(set = 0, binding = 1) uniform sampler linearSampler;

// unused, but required for GBuffer pipelines
layout(set = 0, binding = 2) buffer MaterialBuffer {
    MaterialData materials[MAX_MATERIALS];
};

#include "grid.glsl"

layout(location = 0) in vec2 inVertexPosition;
layout(location = 1) in vec3 inViewPosition;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out uint intProperty;
layout(location = 4) out uvec4 entityID;

void main() {
    vec2 positionInCell = mod(inVertexPosition, grid.cellSize);
    if(positionInCell.x < 0)
    {
        positionInCell.x = grid.size + positionInCell.x;
    }
    if(positionInCell.y < 0)
    {
        positionInCell.y = grid.size + positionInCell.y;
    }
    vec2 thickness = vec2(grid.linePixelWidth / grid.screenSize); // TODO: customisable
    vec2 borderDistance = (1-step(positionInCell, thickness)) * step(positionInCell, vec2(grid.cellSize)-thickness);

    if(borderDistance.x > thickness.x && borderDistance.y > thickness.y) {
        discard;
    }

    outColor = grid.color;
    outViewPosition = vec4(inViewPosition, 1.0);
    outNormal = vec4(0, 0, 0, 0);
    intProperty = 0;
    entityID = uvec4(0);
}