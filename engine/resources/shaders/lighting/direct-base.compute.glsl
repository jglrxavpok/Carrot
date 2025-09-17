#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "includes/lights.glsl"
#include "includes/materials.glsl"
#include <includes/gbuffer.glsl>

#extension GL_EXT_control_flow_attributes: enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

const uint LOCAL_SIZE_X = 32;
const uint LOCAL_SIZE_Y = 32;
layout (local_size_x = LOCAL_SIZE_X) in;
layout (local_size_y = LOCAL_SIZE_Y) in;

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    bool hasTLAS; // handle special cases where no raytraceable geometry is present in the scene
#endif
} push;

#include "includes/gbuffer_input.glsl"
#include <includes/camera.glsl>

DEFINE_GBUFFER_INPUTS(0)
#include "includes/gbuffer_unpack.glsl"

DEFINE_CAMERA_SET(1)
LIGHT_SET(2)
MATERIAL_SYSTEM_SET(4)

#include <lighting/brdf.glsl>
#include <lighting/gi/gi-interface.include.glsl>

layout(rgba32f, set = 5, binding = 0) uniform writeonly image2D outDirectLightingImage;

// needs to be included after LIGHT_SET macro & RT data
#include <lighting/base.common.glsl>

vec3 calculateDirectLighting(inout RandomSampler rng, vec3 albedo, vec3 worldPos, vec3 emissive, vec3 normal, vec3 tangent, vec2 metallicRoughness, bool raytracing) {
    const vec3 cameraPos = (cbo.inverseView * vec4(0, 0, 0, 1)).xyz;

    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
        #endif
        vec3 lightContribution = emissive + lights.ambientColor;
        for (uint i = 0; i < activeLights.count; i++) {
            uint lightIndex = activeLights.indices[i];
            lightContribution += computeLightContribution(lightIndex, worldPos, normal);
        }
        return lightContribution;
        #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {
        vec3 lightContribution = emissive + lights.ambientColor;
        vec3 toCamera = worldPos - cameraPos;
        vec3 incomingRay = normalize(toCamera);
        float distanceToCamera = length(toCamera);
        const float MAX_LIGHT_DISTANCE = 5000.0f; /* TODO: specialization constant? compute properly?*/
        const vec3 rayOrigin = worldPos; // avoid self intersection

        float lightPDF = 1.0f;
        vec3 V = -incomingRay;
        vec3 N = normalize(normal);

        PbrInputs pbr;
        pbr.alpha = metallicRoughness.y * metallicRoughness.y;
        pbr.metallic = metallicRoughness.x;
        pbr.baseColor = albedo;
        pbr.V = V;
        pbr.N = N;
        pbr.NdotV = abs(dot(N, V));

        vec3 lightAtPoint = computeDirectLightingFromLights(/*inout*/rng, /*inout*/lightPDF, pbr, rayOrigin, MAX_LIGHT_DISTANCE);
        return lightAtPoint * lightPDF;
    }
    #endif
}

void main() {
    vec4 outColorWorld;

    const ivec2 currentCoords = ivec2(gl_GlobalInvocationID);
    const ivec2 outputSize = imageSize(outDirectLightingImage);

    if(currentCoords.x >= outputSize.x
    || currentCoords.y >= outputSize.y) {
        return;
    }

    const vec2 inUV = vec2(currentCoords) / vec2(outputSize);

    // TODO: load directly
    float currDepth = texture(sampler2D(gDepth, nearestSampler), inUV).r;
    vec4 outDirectLighting = vec4(0.0);

    float distanceToCamera;
    if(currDepth < 1.0) {
        RandomSampler rng;

        GBuffer gbuffer = unpackGBuffer(inUV);
        vec3 albedo = vec3(1);//gbuffer.albedo.rgb;
        vec4 hWorldPos = cbo.inverseView * vec4(gbuffer.viewPosition, 1.0);
        vec3 worldPos = hWorldPos.xyz / hWorldPos.w;

        // TODO: store directly in CBO
        mat3 cboNormalView = transpose(inverse(mat3(cbo.view)));
        mat3 inverseNormalView = inverse(cboNormalView);
        vec3 normal = inverseNormalView * gbuffer.viewTBN[2];
        vec3 tangent = inverseNormalView * gbuffer.viewTBN[0];
        vec2 metallicRoughness = vec2(gbuffer.metallicness, gbuffer.roughness);

        initRNG(rng, inUV, push.frameWidth, push.frameHeight, push.frameCount);


        vec3 emissiveColor = gbuffer.emissiveColor;
#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 1; // TODO: more than one sample if light importance sampling
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

        [[dont_unroll]] for(int i = 0; i < SAMPLE_COUNT; i++) {
            vec3 gi;
            vec3 r;
            outDirectLighting.rgb += calculateDirectLighting(rng, albedo, worldPos, emissiveColor, normal, tangent, metallicRoughness, true);
        }
        outDirectLighting.rgb *= INV_SAMPLE_COUNT;
#else
        outDirectLighting.rgb = calculateDirectLighting(rng, albedo, worldPos, emissiveColor, normal, tangent, metallicRoughness, false);
#endif

        distanceToCamera = length(gbuffer.viewPosition);
    } else {
        vec4 viewSpaceDir = cbo.inverseNonJitteredProjection * vec4(inUV.x*2-1, inUV.y*2-1, 0.0, 1);
        vec3 worldViewDir = mat3(cbo.inverseView) * viewSpaceDir.xyz;

        const mat3 rot = mat3(
            vec3(1.0, 0.0, 0.0),
            vec3(0.0, 0.0, -1.0),
            vec3(0.0, 1.0, 0.0)
        );
        vec3 skyboxRGB = texture(gSkybox3D, (rot) * worldViewDir).rgb;

        outDirectLighting = vec4(skyboxRGB.rgb,1.0);
        distanceToCamera = 1.0f/0.0f;
    }

    float fogFactor = clamp((distanceToCamera - lights.fogDistance) / lights.fogDepth, 0, 1);
    outDirectLighting.rgb = mix(outDirectLighting.rgb, lights.fogColor, fogFactor);

    outDirectLighting.a = 1.0;
    imageStore(outDirectLightingImage, currentCoords, outDirectLighting);
}