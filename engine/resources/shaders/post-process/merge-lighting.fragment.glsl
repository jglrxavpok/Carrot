#include <includes/camera.glsl>
#include <includes/sampling.glsl>
#include <includes/gbuffer_input.glsl>
#include "includes/debugparams.glsl"
#include "includes/rng.glsl"

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

layout(set = 1, binding = 0) uniform texture2D lighting;
layout(set = 1, binding = 1) uniform texture2D momentsHistoryHistoryLength;
layout(set = 1, binding = 2) uniform texture2D noisyLighting;
DEBUG_OPTIONS_SET(2)
DEFINE_CAMERA_SET(3)

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
} push;

void main() {
    GBuffer g = unpackGBuffer(uv);
    vec4 albedoColor = g.albedo;
    vec4 lightingColor = texture(sampler2D(lighting, gLinearSampler), uv);

    float currDepth = texture(sampler2D(gDepth, gLinearSampler), uv).r;

    // debug rendering
    if(debug.gBufferType != DEBUG_GBUFFER_DISABLED) {
        vec3 worldPos = (cbo.inverseView * vec4(g.viewPosition, 1.0)).xyz;

        if(debug.gBufferType == DEBUG_GBUFFER_ALBEDO) {
            outColor = albedoColor;
        } else if(debug.gBufferType == DEBUG_GBUFFER_DEPTH) {
            outColor = vec4(currDepth, currDepth, currDepth, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_POSITION) {
            outColor = vec4(g.viewPosition, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_NORMAL) {
            outColor = vec4(g.viewTBN[2], 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_METALLIC_ROUGHNESS) {
            outColor = vec4(g.metallicness, g.roughness, 0.0, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_EMISSIVE) {
            outColor = vec4(g.emissiveColor, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_RANDOMNESS) {
            RandomSampler rng;

            initRNG(rng, uv, push.frameWidth, push.frameHeight, push.frameCount);
            outColor = vec4(sampleNoise(rng).rrr, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_TANGENT) {
            outColor = vec4(g.viewTBN[0], 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_LIGHTING) {
            outColor = vec4(lightingColor.rgb, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_NOISY_LIGHTING) {
            outColor = vec4(texture(sampler2D(noisyLighting, gLinearSampler), uv).rgb, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_MOTION) {
            outColor = vec4(abs(g.motionVector), 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_MOMENTS) {
            outColor = texture(sampler2D(momentsHistoryHistoryLength, gNearestSampler), uv);
        } else if(debug.gBufferType == DEBUG_GBUFFER_ENTITYID) {
            outColor = vec4(g.entityID) / 255.0f;
        } else {
            outColor = vec4(uv, 0.0, 1.0);
        }

        if(isnan(outColor.r)
        || isnan(outColor.g)
        || isnan(outColor.b)
        || isnan(outColor.a)
        )
        {
            outColor = vec4((sin(push.frameCount * 0.1)+1.0),0,0,1);
        }

        return;
    }

    if(currDepth < 1.0) {
        outColor = vec4(albedoColor.rgb * lightingColor.rgb, 1.0);
    } else {
        outColor = vec4(lightingColor.rgb, 1.0);
    }
}