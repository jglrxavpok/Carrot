#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "includes/lights.glsl"
#include "includes/materials.glsl"
#include <includes/gbuffer.glsl>

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#extension GL_EXT_buffer_reference: enable
#extension GL_EXT_buffer_reference2 : enable
#include "includes/buffers.glsl"
#endif

#extension GL_EXT_control_flow_attributes: enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
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

#include "includes/rng.glsl"

layout(rgba32f, set = 5, binding = 0) uniform writeonly image2D outGlobalIlluminationImage;
layout(rgba32f, set = 5, binding = 1) uniform writeonly image2D outFirstBounceViewPositionsImage;
layout(rgba32f, set = 5, binding = 2) uniform writeonly image2D outFirstBounceViewNormalsImage;


#ifdef HARDWARE_SUPPORTS_RAY_TRACING
layout(set = 6, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 6, binding = 1) uniform texture2D noiseTexture;

layout(set = 6, binding = 2, scalar) readonly buffer Geometries {
    Geometry geometries[];
};

layout(set = 6, binding = 3, scalar) readonly buffer RTInstances {
    Instance instances[];
};
#endif

// needs to be included after LIGHT_SET macro & RT data
#include "includes/lighting.glsl"

void main() {
    vec4 outColorWorld;

    const ivec2 currentCoords = ivec2(gl_GlobalInvocationID);
    const ivec2 outputSize = imageSize(outGlobalIlluminationImage);

    if(currentCoords.x >= outputSize.x
    || currentCoords.y >= outputSize.y) {
        return;
    }

    const vec2 inUV = vec2(currentCoords) / vec2(outputSize);

    // TODO: load directly
    float currDepth = texture(sampler2D(gDepth, nearestSampler), inUV).r;

    vec4 outGlobalIllumination;
    vec4 outFirstBounceViewPositions;
    vec4 outFirstBounceViewNormals;

    float distanceToCamera;
    if(currDepth < 1.0) {
        RandomSampler rng;

        GBufferSmall gbuffer = unpackGBufferTest(inUV);
        vec4 hWorldPos = cbo.inverseView * vec4(gbuffer.viewPosition, 1.0);
        vec3 worldPos = hWorldPos.xyz / hWorldPos.w;

        // TODO: store directly in CBO
        mat3 cboNormalView = transpose(inverse(mat3(cbo.view)));
        mat3 inverseNormalView = inverse(cboNormalView);
        vec3 normal = inverseNormalView * gbuffer.viewN;
        vec3 tangent = inverseNormalView * gbuffer.viewT;
        vec2 metallicRoughness = vec2(gbuffer.metallicness, gbuffer.roughness);

        initRNG(rng, inUV, push.frameWidth, push.frameHeight, push.frameCount);


        vec3 emissiveColor = gbuffer.emissiveColor;
#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 2; // TODO: configurable sample count?
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

        vec3 globalIllumination = vec3(0.0);
        vec3 reflections = vec3(0.0);

        vec3 firstBounceWorldPos = vec3(0.0);
        vec3 firstBounceNormal = vec3(0.0);
        [[dont_unroll]] for(int i = 0; i < SAMPLE_COUNT; i++) {
            vec3 gi;
            vec3 r;
            LightingResult result = calculateGI(rng, worldPos, emissiveColor, normal, tangent, metallicRoughness, true);
            firstBounceWorldPos += result.position;
            firstBounceNormal += result.normal;
            globalIllumination += result.color.rgb;
        }
        outGlobalIllumination.rgb = globalIllumination * INV_SAMPLE_COUNT;

        outFirstBounceViewPositions = vec4((cbo.view * vec4(firstBounceWorldPos * INV_SAMPLE_COUNT, 1)).xyz, 1);

        outFirstBounceViewNormals = vec4(cboNormalView * normalize(firstBounceNormal * INV_SAMPLE_COUNT), 1.0);
#else
        vec3 gi;
        vec3 r;
        LightingResult result = calculateGI(rng, worldPos, emissiveColor, normal, tangent, metallicRoughness, false);
        outGlobalIllumination.rgb = result.color.rgb;
        outFirstBounceViewPositions = vec4((cbo.view * vec4(result.position, 1)).xyz, 1);
        outFirstBounceViewNormals = vec4(cboNormalView * result.normal, 1.0);
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

        outGlobalIllumination = vec4(skyboxRGB.rgb,1.0);
        distanceToCamera = 1.0f/0.0f;
        outFirstBounceViewPositions = vec4(0,0,0, 0);
        outFirstBounceViewNormals = vec4(0,0,0, 1.0);
    }

    float fogFactor = clamp((distanceToCamera - lights.fogDistance) / lights.fogDepth, 0, 1);
    outGlobalIllumination.rgb = mix(outGlobalIllumination.rgb, lights.fogColor, fogFactor);

    outGlobalIllumination.a = 1.0;
    imageStore(outGlobalIlluminationImage, currentCoords, outGlobalIllumination);
    imageStore(outFirstBounceViewPositionsImage, currentCoords, outFirstBounceViewPositions);
    imageStore(outFirstBounceViewNormalsImage, currentCoords, outFirstBounceViewNormals);
}