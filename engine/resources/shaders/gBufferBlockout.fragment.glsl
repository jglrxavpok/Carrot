#extension GL_EXT_nonuniform_qualifier : enable

#include <includes/camera.glsl>
#include "draw_data.glsl"
#include <includes/gbuffer.glsl>
#include "includes/gbuffer_output.glsl"
#include "includes/materials.glsl"

DEFINE_CAMERA_SET(0)
MATERIAL_SYSTEM_SET(1)
DEFINE_PER_DRAW_BUFFER(2)

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 viewPosition;
layout(location = 3) in vec3 previousFrameViewPosition;
layout(location = 4) flat in uvec4 uuid;
layout(location = 5) in vec3 T;
layout(location = 6) in vec3 N;
layout(location = 7) flat in float bitangentSign;
layout(location = 8) flat in mat4 inModelview;
layout(location = 12) flat in int inDrawID;

void main() {
    DrawData instanceDrawData = perDrawData.drawData[perDrawDataOffsets.offset+inDrawID];

    Material material = materials[instanceDrawData.materialIndex];

    vec3 worldPos = (cbo.inverseView * vec4(viewPosition, 1)).xyz;

    ivec3 roundedPos = ivec3(round(worldPos));

    // color of splash screen (FF8146)
    vec4 color1 = vec4(0xFF / 255.0f, 0x81 / 255.0f, 0x46 / 255.0f, 1);
    vec4 color2 = vec4(0x30 / 255.0f, 0x30 / 255.0f, 0x30 / 255.0f, 1);
    vec4 texColor = ((roundedPos.x + roundedPos.y + roundedPos.z) & 1) == 0 ? color1 : color2;

    texColor *= material.baseColor;
    texColor *= vColor;

    texColor.a = 1.0;
    if(texColor.a < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer(inModelview);

    o.albedo = vec4(texColor.rgb, texColor.a);
    o.viewPosition = viewPosition;

    o.intProperty = IntPropertiesRayTracedLighting;
    o.entityID = uuid;

    vec4 clipPos = cbo.nonJitteredProjection * vec4(viewPosition, 1.0);
    vec4 previousClipPos = previousFrameCBO.nonJitteredProjection * vec4(previousFrameViewPosition, 1.0);
    o.motionVector = previousClipPos.xyz/previousClipPos.w - clipPos.xyz/clipPos.w;

    outputGBuffer(o, inModelview);
}