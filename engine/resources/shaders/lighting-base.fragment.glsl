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

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;

    bool hasTLAS; // handle special cases where no raytraceable geometry is present in the scene
} push;

#include "includes/gbuffer_input.glsl"
#include <includes/camera.glsl>

DEFINE_GBUFFER_INPUTS(0)
#include "includes/gbuffer_unpack.glsl"

DEFINE_CAMERA_SET(1)
LIGHT_SET(2)
MATERIAL_SYSTEM_SET(4)

#include "includes/rng.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
layout(set = 5, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 5, binding = 1) uniform texture2D noiseTexture;

layout(set = 5, binding = 2, scalar) buffer Geometries {
    Geometry geometries[];
};

layout(set = 5, binding = 3, scalar) buffer RTInstances {
    Instance instances[];
};
#endif

// needs to be included after LIGHT_SET macro & RT data
#include "includes/lighting.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 outColorWorld;

    vec2 uv = inUV;

    float currDepth = texture(sampler2D(gDepth, nearestSampler), uv).r;


    float distanceToCamera;
    if(currDepth < 1.0) {
        RandomSampler rng;

        GBuffer gbuffer = unpackGBuffer(uv);
        vec4 hWorldPos = cbo.inverseView * vec4(gbuffer.viewPosition, 1.0);
        vec3 worldPos = hWorldPos.xyz / hWorldPos.w;

        vec3 normal = mat3(cbo.inverseView) * gbuffer.viewTBN[2];
        vec3 tangent = mat3(cbo.inverseView) * gbuffer.viewTBN[0];
        vec2 metallicRoughness = vec2(gbuffer.metallicness, gbuffer.roughness);

        initRNG(rng, uv, push.frameWidth, push.frameHeight, push.frameCount);


#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 4; // TODO: configurable sample count?
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

        vec3 l = vec3(0.0);
        for(int i = 0; i < SAMPLE_COUNT; i++) {
            l += calculateLighting(rng, worldPos, gbuffer.emissiveColor, normal, tangent, metallicRoughness, true);
        }
        outColorWorld.rgb = l * INV_SAMPLE_COUNT;
#else
        outColorWorld.rgb = calculateLighting(rng, worldPos, gbuffer.emissiveColor, normal, tangent, metallicRoughness, false);
#endif

        distanceToCamera = length(gbuffer.viewPosition);
    } else {
        vec4 viewSpaceDir = cbo.inverseNonJitteredProjection * vec4(uv.x*2-1, uv.y*2-1, 0.0, 1);
        vec3 worldViewDir = mat3(cbo.inverseView) * viewSpaceDir.xyz;

        const mat3 rot = mat3(
            vec3(1.0, 0.0, 0.0),
            vec3(0.0, 0.0, -1.0),
            vec3(0.0, 1.0, 0.0)
        );
        vec3 skyboxRGB = texture(gSkybox3D, (rot) * worldViewDir).rgb;

        outColorWorld = vec4(skyboxRGB.rgb,1.0);
        distanceToCamera = 1.0f/0.0f;
    }

    float fogFactor = clamp((distanceToCamera - lights.fogDistance) / lights.fogDepth, 0, 1);
    outColorWorld.rgb = mix(outColorWorld.rgb, lights.fogColor, fogFactor);


    outColorWorld.a = 1.0;
    outColor = outColorWorld;
}