#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/materials.glsl"
#include "includes/billboards.glsl"

MATERIAL_SYSTEM_SET(0)

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 viewPosition;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out uint intProperty;
layout(location = 4) out uvec4 entityID;


void main() {
    vec4 texColor = texture(sampler2D(textures[billboard.textureID], linearSampler), uv) * vec4(billboard.color, 1.0);
    if(texColor.a < 0.01) {
        discard;
    }

    outColor = vec4(texColor.rgb, 1.0);
    outViewPosition = vec4(viewPosition, 1.0);

    outNormal = vec4(0,1,0,0);

    intProperty = 0;
    entityID = billboard.uuid;
}