#include <includes/camera.glsl>
#include <includes/sampling.glsl>
#include <includes/gbuffer.glsl>
#include <includes/gbuffer_input.glsl>
#include "includes/debugparams.glsl"
#include "includes/materials.glsl"

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

layout(set = 1, binding = 0) uniform texture2D directLighting;
layout(set = 1, binding = 1) uniform texture2D aoLighting;
layout(set = 1, binding = 2) uniform texture2D reflections;
layout(set = 1, binding = 3) uniform texture2D gi;
layout(set = 1, binding = 4) uniform texture2D visibilityBufferDebug[DEBUG_VISIBILITY_BUFFER_LAST - DEBUG_VISIBILITY_BUFFER_FIRST+1];
DEBUG_OPTIONS_SET(2)
DEFINE_CAMERA_SET(3)

MATERIAL_SYSTEM_SET(4)

#include <includes/rng.glsl>

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
} push;

bool earlyExits(uint debugMode) {
    return debugMode != DEBUG_GBUFFER_DISABLED;
}

void main() {
    GBuffer g = unpackGBuffer(uv);
    vec4 albedoColor = g.albedo;
    vec3 lightingColor = texture(sampler2D(directLighting, gLinearSampler), uv).rgb;
    vec3 reflectionsColor = texture(sampler2D(reflections, gLinearSampler), uv).rgb;

    float ao = texture(sampler2D(aoLighting, gLinearSampler), uv).r;

    float currDepth = texture(sampler2D(gDepth, gLinearSampler), uv).r;

    // debug rendering
    if(earlyExits(debug.gBufferType)) {
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
        } else if(debug.gBufferType == DEBUG_GBUFFER_MOTION) {
            outColor = vec4(g.motionVector.rgb, 1.0);
        } else if(debug.gBufferType == DEBUG_GBUFFER_ENTITYID) {
            outColor = vec4(g.entityID) / 255.0f;
        } else if(debug.gBufferType >= DEBUG_VISIBILITY_BUFFER_FIRST && debug.gBufferType <= DEBUG_VISIBILITY_BUFFER_LAST) {
            uint debugIndex = debug.gBufferType - DEBUG_VISIBILITY_BUFFER_FIRST;
            outColor = texture(sampler2D(visibilityBufferDebug[debugIndex], gLinearSampler), uv);
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

    vec3 finalOpaqueColor;
    if(currDepth < 1.0) {
        vec3 giColor = texture(sampler2D(gi, gLinearSampler), uv).rgb;
        lightingColor += giColor;
        finalOpaqueColor = mix(albedoColor.rgb * lightingColor, reflectionsColor, g.metallicness);
        finalOpaqueColor *= ao*ao;
        finalOpaqueColor += g.emissiveColor;
    } else {
        finalOpaqueColor = lightingColor.rgb;
    }

    outColor = vec4(finalOpaqueColor, 1.0);
}