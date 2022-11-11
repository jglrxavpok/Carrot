#extension GL_EXT_nonuniform_qualifier : enable

#include "draw_data.glsl"
#include "includes/gbuffer.glsl"
#include "includes/materials.glsl"

MATERIAL_SYSTEM_SET(1)

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) in vec3 viewNormal;
layout(location = 5) flat in uvec4 inUUID; // TODO: unused at the moment

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out uint intProperty;
layout(location = 4) out uvec4 entityID;
layout(location = 5) out vec4 roughnessMetallic;
layout(location = 6) out vec4 emissive;

void main() {
    DrawData instanceDrawData = drawDataPush.drawData[0]; // TODO: instancing
    //#define material (materials[instanceDrawData.materialIndex])

    Material material = materials[instanceDrawData.materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    if(texColor.a < 0.01) {
        discard;
    }
    outColor = texColor * fragColor * instanceColor;
    outViewPosition = vec4(viewPosition, 1.0);
    outNormal = vec4(viewNormal, 1.0);
    intProperty = IntPropertiesRayTracedLighting;
    entityID = uvec4(instanceDrawData.uuid0, instanceDrawData.uuid1, instanceDrawData.uuid2, instanceDrawData.uuid3);
    roughnessMetallic = vec4(0.0);
    emissive = vec4(0.0);
}