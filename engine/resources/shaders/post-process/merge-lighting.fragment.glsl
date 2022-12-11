#include <includes/camera.glsl>
#include <includes/sampling.glsl>
#include <includes/gbuffer_input.glsl>
#include "includes/debugparams.glsl"
#include "includes/rng.glsl"

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

layout(set = 1, binding = 0) uniform texture2D lighting;
layout(set = 1, binding = 1) uniform texture2D momentsHistoryHistoryLength;
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

        vec3 normal = normalize((cbo.inverseView * vec4(g.viewNormal, 0.0)).xyz);
        vec3 tangent = normalize((cbo.inverseView * vec4(g.viewTangent, 0.0)).xyz);

        if(debug.gBufferType == DEBUG_GBUFFER_ALBEDO) {
            outColor = albedoColor;
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_DEPTH) {
            outColor = vec4(currDepth, currDepth, currDepth, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_POSITION) {
            outColor = vec4(g.viewPosition, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_NORMAL) {
            outColor = vec4(g.viewNormal, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_METALLIC_ROUGHNESS) {
            outColor = vec4(g.metallicness, g.roughness, 0.0, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_EMISSIVE) {
            outColor = vec4(g.emissiveColor, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_RANDOMNESS) {
            RandomSampler rng;

            initRNG(rng, uv, push.frameWidth, push.frameHeight, push.frameCount);
            outColor = vec4(sampleNoise(rng).rrr, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_TANGENT) {
            outColor = vec4(g.viewTangent, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_LIGHTING) {
            outColor = vec4(lightingColor.rgb, 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_MOTION) {
            outColor = vec4(abs(g.motionVector), 1.0);
            return;
        } else if(debug.gBufferType == DEBUG_GBUFFER_MOMENTS) {
            outColor = texture(sampler2D(momentsHistoryHistoryLength, gNearestSampler), uv);
            return;
        }

        outColor = vec4(uv, 0.0, 1.0);
        return;
    }

    if(currDepth < 1.0) {
        outColor = vec4(albedoColor.rgb * lightingColor.rgb, 1.0);
    } else {
        outColor = vec4(lightingColor.rgb, 1.0);
    }
}