#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "includes/gbuffer.glsl"

layout(set = 0, binding = 0) uniform texture2D albedo;
layout(set = 0, binding = 1) uniform texture2D depth;
layout(set = 0, binding = 2) uniform texture2D viewPos;
layout(set = 0, binding = 3) uniform texture2D viewNormals;
layout(set = 0, binding = 4) uniform usampler2D intPropertiesInput;
layout(set = 0, binding = 5) uniform texture2D rayTracedLighting;
layout(set = 0, binding = 6) uniform texture2D uiRendered;
layout(set = 0, binding = 7) uniform sampler linearSampler;

layout(set = 0, binding = 8) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(set = 0, binding = 9) uniform Debug {
    #include "debugparams.glsl"
} debug;

layout(set = 0, binding = 10) uniform texture2D skyboxTexture;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

const vec3 sunDirection = vec3(1,1,1);

void main() {
    vec4 outColorWorld;

    const vec3 nSunDirection = normalize(sunDirection);
    vec4 fragmentColor = texture(sampler2D(albedo, linearSampler), uv);
    vec3 normal = (cbo.inverseView * texture(sampler2D(viewNormals, linearSampler), uv)).xyz;
    vec3 lighting = texture(sampler2D(rayTracedLighting, linearSampler), uv).rgb;
    vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;

    uint intProperties = texture(intPropertiesInput, uv).r;
    float currDepth = texture(sampler2D(depth, linearSampler), uv).r;
    if(currDepth < 1.0) {
        if((intProperties & IntPropertiesRayTracedLighting) == IntPropertiesRayTracedLighting) {
            outColorWorld = vec4(fragmentColor.rgb*lighting, fragmentColor.a);
        } else {
            outColorWorld = fragmentColor;
        }
    } else {
        outColorWorld = vec4(skyboxRGB, fragmentColor.a);
    }

    vec4 outColorUI = texture(sampler2D(uiRendered, linearSampler), uv);
    float alpha01 = (1 - outColorUI.a)*outColorWorld.a + outColorUI.a;
    vec4 renderedColor = vec4(((1-outColorUI.a) * outColorWorld.a * outColorWorld.rgb + outColorUI.a*outColorUI.rgb) / alpha01, 1.0);
    outColor = vec4(renderedColor.rgb, 1.0);
}