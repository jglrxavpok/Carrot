#extension GL_EXT_nonuniform_qualifier : enable

#include <includes/camera.glsl>
#include "draw_data.glsl"
#include <includes/gbuffer.glsl>
#include "includes/gbuffer_output.glsl"
#include "includes/materials.glsl"
#include "includes/viewport.glsl"


DEFINE_CAMERA_SET(0)
MATERIAL_SYSTEM_SET(1)
DEFINE_VIEWPORT_CONSTANT(2)
DEFINE_PER_DRAW_BUFFER(3)

#include "includes/rng.glsl"

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
    uint albedoTexture = nonuniformEXT(material.albedo);
    uint normalMap = nonuniformEXT(material.normalMap);
    uint emissiveTexture = nonuniformEXT(material.emissive);
    uint metallicRoughnessTexture = nonuniformEXT(material.metallicRoughness);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    texColor *= material.baseColor;
    texColor *= vColor;

    if(dither(uvec2(gl_FragCoord.xy)) >= texColor.a) {
        discard;
    }
    texColor.a = 1.0;

    GBuffer o = initGBuffer(inModelview);

    o.albedo = vec4(texColor.rgb, texColor.a);
    o.viewPosition = viewPosition;

    vec3 N_ = normalize(N);
    vec3 T_ = normalize(T - dot(T, N_) * N_);

    vec3 B_ = normalize(bitangentSign * cross(N_, T_));

    vec3 mappedNormal = texture(sampler2D(textures[normalMap], linearSampler), uv).xyz;
    mappedNormal = mappedNormal * 2 -1;
    mappedNormal = normalize(mappedNormal.x * T_ + mappedNormal.y * B_ + mappedNormal.z * N_);

    N_ = mappedNormal;
    T_ = normalize(T - dot(T, N_) * N_);
    B_ = normalize(bitangentSign * cross(N_, T_));

    o.viewTBN = mat3(T_, B_, N_);

    o.intProperty = IntPropertiesRayTracedLighting;
    o.entityID = uuid;

    vec2 metallicRoughness = texture(sampler2D(textures[metallicRoughnessTexture], linearSampler), uv).bg * material.metallicRoughnessFactor;
    o.metallicness = metallicRoughness.x;
    o.roughness = metallicRoughness.y;
    o.emissiveColor = texture(sampler2D(textures[emissiveTexture], linearSampler), uv).rgb * material.emissiveColor;

    vec4 clipPos = cbo.nonJitteredProjection * vec4(viewPosition, 1.0);
    vec4 previousClipPos = previousFrameCBO.nonJitteredProjection * vec4(previousFrameViewPosition, 1.0);
    o.motionVector = previousClipPos.xyz/previousClipPos.w - clipPos.xyz/clipPos.w;

    outputGBuffer(o, inModelview);
}