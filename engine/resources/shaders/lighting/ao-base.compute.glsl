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

#include "includes/rng.glsl"

layout(set = 5, binding = 0) uniform writeonly image2D outAOImage;

// needs to be included after LIGHT_SET macro & RT data
#include <lighting/base.common.glsl>

float calculateAO(inout RandomSampler rng, vec3 worldPos, mat3 tbn, bool raytracing) {
    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
        #endif
        // TODO: attempt SSAO?
        return 1.0f;
        #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {

        vec3 directionTangentSpace = cosineSampleHemisphere(rng);
        vec3 direction = tbn * directionTangentSpace;
        float tMin = 0.001f;
        float tMax = 0.02f; // 2cm
        initRayQuery(worldPos, direction, tMax, tMin);
        bool noIntersection = traceShadowRay();
        return noIntersection ? 1.0f : 0.0f;
    }
    #endif
}

void main() {
    vec4 outColorWorld;

    const ivec2 currentCoords = ivec2(gl_GlobalInvocationID);
    const ivec2 outputSize = imageSize(outAOImage);

    if(currentCoords.x >= outputSize.x
    || currentCoords.y >= outputSize.y) {
        return;
    }

    const vec2 inUV = vec2(currentCoords) / vec2(outputSize);

    // TODO: load directly
    float currDepth = texture(sampler2D(gDepth, nearestSampler), inUV).r;
    float outAO = 1.0;
    if(currDepth < 1.0) {
        RandomSampler rng;

        GBufferSmall gbuffer = unpackGBufferLightTest(inUV);
        vec4 hWorldPos = cbo.inverseView * vec4(gbuffer.viewPosition, 1.0);
        vec3 worldPos = hWorldPos.xyz / hWorldPos.w;

        // TODO: store directly in CBO
        mat3 cboNormalView = transpose(inverse(mat3(cbo.view)));
        mat3 inverseNormalView = inverse(cboNormalView);
        vec3 normal = inverseNormalView * gbuffer.viewN;
        vec3 tangent = inverseNormalView * gbuffer.viewT;

        initRNG(rng, inUV, push.frameWidth, push.frameHeight, push.frameCount);

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 1;
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

        vec3 T = tangent;
        vec3 N = normal;
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(T, N);
        mat3 tbn = mat3(T, B, N);

        outAO = 0.0f;
        [[dont_unroll]] for(int i = 0; i < SAMPLE_COUNT; i++) {
            vec3 gi;
            vec3 r;
            outAO += calculateAO(rng, worldPos, tbn, true);
        }
        outAO *= INV_SAMPLE_COUNT;
#endif
    }

    imageStore(outAOImage, currentCoords, vec4(outAO.rrr, 1.0));
}