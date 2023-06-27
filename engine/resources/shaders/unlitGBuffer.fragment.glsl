#extension GL_EXT_nonuniform_qualifier : enable

#include <includes/camera.glsl>
#include "draw_data.glsl"
#include "includes/materials.glsl"

DEFINE_CAMERA_SET(0)
MATERIAL_SYSTEM_SET(1)
DEFINE_PER_DRAW_BUFFER(2)

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) in vec3 previousFrameViewPosition;
layout(location = 5) in vec3 _unused;
layout(location = 6) flat in uvec4 uuid;
layout(location = 7) in vec3 T;
layout(location = 8) in vec3 N;
layout(location = 9) flat in float bitangentSign;
layout(location = 10) flat in mat4 inModelview;
layout(location = 14) flat in int inDrawID;

layout(location = 0) out vec4 outColor;

void main() {
    DrawData instanceDrawData = perDrawData.drawData[perDrawDataOffsets.offset+inDrawID];
    Material material = materials[instanceDrawData.materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    texColor *= material.baseColor;
    texColor *= instanceColor;
    texColor.rgb *= fragColor;

    texColor.a = 1.0;
    if(texColor.a < 0.01) {
        discard;
    }

    outColor = vec4(texColor.rgb, texColor.a * instanceColor.a);
}