#extension GL_EXT_nonuniform_qualifier : enable

#include "draw_data.glsl"
#include "includes/gbuffer_output.glsl"
#include "includes/materials.glsl"


MATERIAL_SYSTEM_SET(1)

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) in vec3 viewNormal; // TODO: remove? no longer used
layout(location = 5) flat in uvec4 uuid;
layout(location = 6) in mat3 TBN;

void main() {
    DrawData instanceDrawData = drawDataPush.drawData[0]; // TODO: instancing
    //#define material (materials[instanceDrawData.materialIndex])

    Material material = materials[instanceDrawData.materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    uint normalMap = nonuniformEXT(material.normalMap);
    uint emissiveTexture = nonuniformEXT(material.emissive);
    uint metallicRoughnessTexture = nonuniformEXT(material.metallicRoughness);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    texColor *= material.baseColor;
    texColor.a = 1.0;
    if(texColor.a < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer();

    o.albedo = vec4(texColor.rgb * fragColor * instanceColor.rgb * material.baseColor.rgb, texColor.a * instanceColor.a);
    o.viewPosition = viewPosition;

    vec3 mappedNormal = texture(sampler2D(textures[normalMap], linearSampler), uv).xyz;
    mappedNormal = normalize(mappedNormal *2 -1);
    o.viewNormal = normalize(TBN * mappedNormal);
    o.viewTangent = normalize(TBN * vec3(1.0, 0.0, 0.0)); // TODO: deferred texturing

    o.intProperty = IntPropertiesRayTracedLighting;
    o.entityID = uuid;

    vec2 metallicRoughness = texture(sampler2D(textures[metallicRoughnessTexture], linearSampler), uv).bg * material.metallicRoughnessFactor;
    o.metallicness = metallicRoughness.x;
    o.roughness = metallicRoughness.y;
    o.emissiveColor = texture(sampler2D(textures[emissiveTexture], linearSampler), uv).rgb * material.emissiveColor;

    outputGBuffer(o);
}