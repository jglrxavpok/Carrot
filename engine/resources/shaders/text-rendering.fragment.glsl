#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/materials.glsl"
#include "includes/gbuffer.glsl"
#include "draw_data.glsl"

MATERIAL_SYSTEM_SET(0)

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) in vec3 viewNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out uint intProperty;

void main() {
    DrawData instanceDrawData = drawDataPush.drawData[0]; // TODO: instancing
    Material material = materials[instanceDrawData.materialIndex];
    uint diffuseTexture = nonuniformEXT(material.diffuseTexture);
    float color = texture(sampler2D(textures[diffuseTexture], linearSampler), uv).r;
    if(color < 0.01) {
        discard;
    }
    outColor = vec4(1.0, 1.0, 1.0, color) * fragColor * instanceColor;
    outViewPosition = vec4(viewPosition, 1.0);
    outNormal = viewNormal;
    intProperty = IntPropertiesRayTracedLighting;
}