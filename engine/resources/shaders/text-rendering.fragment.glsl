#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/materials.glsl"
#include "includes/gbuffer_output.glsl"
#include "draw_data.glsl"

MATERIAL_SYSTEM_SET(0)
// 1 used by vertex shader
DEFINE_PER_DRAW_BUFFER(2)

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) flat in mat4 inModelview;
layout(location = 8) flat in int inDrawID;

void main() {
    DrawData instanceDrawData = perDrawData.drawData[perDrawDataOffsets.offset+inDrawID];
    Material material = materials[instanceDrawData.materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    float color = texture(sampler2D(textures[albedoTexture], linearSampler), uv).r;
    if(color < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer(inModelview);
    o.albedo = vec4(1.0, 1.0, 1.0, color) * fragColor * instanceColor;
    o.viewPosition = viewPosition;
    o.intProperty = IntPropertiesRayTracedLighting;

    outputGBuffer(o, inModelview);
}