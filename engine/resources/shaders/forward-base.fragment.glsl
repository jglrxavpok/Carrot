#extension GL_EXT_nonuniform_qualifier : enable

#include "includes/camera.glsl"
#include "draw_data.glsl"
#include "includes/lights.glsl"
#include "includes/materials.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#extension GL_EXT_buffer_reference: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_buffer_reference2 : enable
#include "includes/buffers.glsl"
#endif
#include "includes/rng.glsl"

DEFINE_CAMERA_SET(0)
LIGHT_SET(1)
MATERIAL_SYSTEM_SET(4)

layout(set = 5, binding = 0) uniform samplerCube gSkybox3D;
layout(set = 5, binding = 1) uniform FakePushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;

    bool hasTLAS;
} push;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
layout(set = 5, binding = 2) uniform accelerationStructureEXT topLevelAS;
layout(set = 5, binding = 3) uniform texture2D noiseTexture;

layout(set = 5, binding = 4, scalar) buffer Geometries {
    Geometry geometries[];
};

layout(set = 5, binding = 5, scalar) buffer RTInstances {
    Instance instances[];
};
#endif

// needs to be included after LIGHT_SET macro & RT data
#include "includes/lighting.glsl"

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

layout(location = 0) out vec4 color;

void main() {
    DrawData instanceDrawData = drawDataPush.drawData[0]; // TODO: instancing
    //#define material (materials[instanceDrawData.materialIndex])

    const Material material = materials[instanceDrawData.materialIndex];
    const uint albedoTexture = material.albedo;
    const uint normalMap = material.normalMap;
    const uint emissiveTexture = material.emissive;
    const uint metallicRoughnessTexture = material.metallicRoughness;

    vec4 startingColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    startingColor *= material.baseColor * instanceColor;
    startingColor.rgb *= fragColor;
    const float alpha = startingColor.a;

    RandomSampler rng;
    initRNG(rng, uv, push.frameWidth, push.frameHeight, push.frameCount);

    const vec3 worldPos = (cbo.inverseView * vec4(viewPosition, 1.0)).xyz;

    vec3 N_ = normalize(N);
    vec3 T_ = normalize(T - dot(T, N_) * N_);

    vec3 B_ = normalize(bitangentSign * cross(N_, T_));

    vec3 mappedNormal = texture(sampler2D(textures[normalMap], linearSampler), uv).xyz;
    mappedNormal = mappedNormal * 2 -1;
    mappedNormal = normalize(mappedNormal.x * T_ + mappedNormal.y * B_ + mappedNormal.z * N_);

    N_ = mappedNormal;
    T_ = normalize(T - dot(T, N_) * N_);
    B_ = normalize(bitangentSign * cross(N_, T_));

    const mat3 tbn = mat3(cbo.inverseView) * mat3(inModelview) * mat3(T_, B_, N_);
    const vec3 tangent = tbn[0];
    const vec3 normal = tbn[2];

    vec2 metallicRoughness = texture(sampler2D(textures[metallicRoughnessTexture], linearSampler), uv).bg * material.metallicRoughnessFactor;
    const vec3 emissiveColor = texture(sampler2D(textures[emissiveTexture], linearSampler), uv).rgb * material.emissiveColor;

    vec3 outColor = vec3(0,0,0);
#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    const int SAMPLE_COUNT = 2; // TODO: configurable sample count?
    const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

    vec3 l = vec3(0.0);
    for(int i = 0; i < SAMPLE_COUNT; i++) {
        l += calculateLighting(rng, worldPos, emissiveColor, normal, tangent, metallicRoughness, true);
    }
    outColor = l * INV_SAMPLE_COUNT;
#else
    outColor = calculateLighting(rng, worldPos, emissiveColor, normal, tangent, metallicRoughness, true);
#endif

    color = vec4(outColor * startingColor.rgb, alpha);
}
