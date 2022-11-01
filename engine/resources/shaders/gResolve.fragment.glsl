#include "includes/gbuffer.glsl"
#include "includes/debugparams.glsl"

layout(input_attachment_index = 0, binding = 0) uniform subpassInput albedo;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput depth;
layout(input_attachment_index = 2, binding = 2) uniform subpassInput viewPos;
layout(input_attachment_index = 3, binding = 3) uniform subpassInput viewNormals;
layout(set = 0, binding = 4) uniform usampler2D intPropertiesInput;
layout(set = 0, binding = 5) uniform texture2D rayTracedLighting;
layout(set = 0, binding = 6) uniform sampler linearSampler;

DEBUG_OPTIONS_SET(3)

layout(set = 0, binding = 10) uniform texture2D skyboxTexture;

layout(set = 1, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

const vec3 sunDirection = vec3(1,1,1);

void main() {
    vec4 outColorWorld;

    const vec3 nSunDirection = normalize(sunDirection);
    vec4 fragmentColor = subpassLoad(albedo);
    vec3 normal = (cbo.inverseView * subpassLoad(viewNormals)).xyz;
    vec3 lighting = texture(sampler2D(rayTracedLighting, linearSampler), uv).rgb;
    vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;

    uint intProperties = texture(intPropertiesInput, uv).r;
    float currDepth = subpassLoad(depth).r;
    if(currDepth < 1.0) {
        if((intProperties & IntPropertiesRayTracedLighting) == IntPropertiesRayTracedLighting) {
            outColorWorld = vec4(fragmentColor.rgb*lighting, fragmentColor.a);
        } else {
            outColorWorld = fragmentColor;
        }
    } else {
        outColorWorld = vec4(skyboxRGB, fragmentColor.a);
    }

    outColor = outColorWorld;
}