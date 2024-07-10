#extension GL_EXT_nonuniform_qualifier : enable

#include "draw_data.glsl"
#include <includes/gbuffer.glsl>
#include "includes/gbuffer_output.glsl"
#include "includes/materials.glsl"

MATERIAL_SYSTEM_SET(1)
DEFINE_PER_DRAW_BUFFER(2)

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) flat in uvec4 inUUID; // TODO: unused at the moment
layout(location = 5) flat in mat4 inModelview;
layout(location = 9) flat in int inDrawID;

void main() {
    DrawData instanceDrawData = perDrawData.drawData[perDrawDataOffsets.offset+inDrawID];
    //#define material (materials[instanceDrawData.materialIndex])

    Material material = materials[instanceDrawData.materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), vec2(uv.x, 1-uv.y)); // TODO: GP Direct
    if(texColor.a < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer(inModelview);
    o.albedo = texColor * fragColor * instanceColor;
    o.viewPosition = viewPosition;
    o.intProperty = IntPropertiesRayTracedLighting;
    o.entityID = uvec4(instanceDrawData.uuid0, instanceDrawData.uuid1, instanceDrawData.uuid2, instanceDrawData.uuid3);

    outputGBuffer(o, inModelview);
}