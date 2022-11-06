#extension GL_EXT_nonuniform_qualifier : enable

#include "draw_data.glsl"
#include "includes/gbuffer.glsl"
#include "includes/materials.glsl"


MATERIAL_SYSTEM_SET(1)

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) in vec3 viewNormal;
layout(location = 5) flat in uvec4 uuid;
layout(location = 6) in mat3 TBN;

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
    uint normalMap = nonuniformEXT(material.normalMap);
    uint emissiveTexture = nonuniformEXT(material.emissive);
    uint roughnessMetallicTexture = nonuniformEXT(material.roughnessMetallic);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    texColor *= material.baseColor;
    texColor.a = 1.0;
    if(texColor.a < 0.01) {
        discard;
    }
    outColor = vec4(texColor.rgb * fragColor * instanceColor.rgb, texColor.a * instanceColor.a);
    outViewPosition = vec4(viewPosition, 1.0);

    vec3 mappedNormal = texture(sampler2D(textures[normalMap], linearSampler), uv).xyz;
    mappedNormal = mappedNormal;
    outNormal = vec4(normalize(TBN * mappedNormal), 1.0);

    intProperty = IntPropertiesRayTracedLighting;
    //entityID = uvec4(instanceDrawData.uuid0, instanceDrawData.uuid1, instanceDrawData.uuid2, instanceDrawData.uuid3);
    entityID = uuid;
    roughnessMetallic = vec4(texture(sampler2D(textures[roughnessMetallicTexture], linearSampler), uv).rg * material.roughnessMetallicFactor, 0.0, 1.0);
    emissive = vec4(texture(sampler2D(textures[emissiveTexture], linearSampler), uv).rgb * material.emissive, 1.0);
}