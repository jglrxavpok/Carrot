#include "includes/gbuffer.glsl"
#include "includes/lights.glsl"

layout(set = 0, binding = 0) uniform texture2D albedo;
layout(set = 0, binding = 1) uniform texture2D depth;
layout(set = 0, binding = 2) uniform texture2D viewPos;
layout(set = 0, binding = 3) uniform texture2D viewNormals;
layout(set = 0, binding = 4) uniform usampler2D intPropertiesInput;
layout(set = 0, binding = 5) uniform texture2D rayTracedLighting;
layout(set = 0, binding = 6) uniform texture2D transparent;
layout(set = 0, binding = 7) uniform texture2D skyboxTexture;
layout(set = 0, binding = 8) uniform sampler linearSampler;
layout(set = 0, binding = 9) uniform sampler nearestSampler;

layout(set = 0, binding = 10) uniform Debug {
    #include "debugparams.glsl"
} debug;

layout(set = 2, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

LIGHT_SET(3)

vec3 calculateLighting(vec3 worldPos, vec3 normal, bool computeShadows) {
    vec3 finalColor = vec3(0.0);

    vec3 lightContribution = vec3(0.0);

    for(uint i = 0; i < lights.count; i++) {
        #define light lights.l[i]

        if(!light.enabled)
        continue;

        // point type
        vec3 lightDirection;

        if(light.type == 0) {
            vec3 lightPosition = light.position;
            lightDirection = normalize(lightPosition-worldPos);
        } else if(light.type == 1) {
            lightDirection = -normalize(light.direction);
        } else {
            // TODO
            lightDirection = -normal;
        }


        if(dot(normal, lightDirection) > 0) {
            // is this point in shadow?
            bool isShadowed = true;

            if(!computeShadows) {
                isShadowed = false;
            }

            /* TODO: reintroduce raytracing
            if(computeShadows) {
                traceRayEXT(topLevelAS, // AS
                    rayFlags, // ray flags
                    0xFF, // cull mask
                    0, // sbtRecordOffset
                    0, // sbtRecordStride
                    1, // missIndex
                    worldPos, // ray origin
                    tMin, // ray min range
                    lightDirection, // ray direction
                    tMax, // ray max range,
                    1 // payload location
                );
            }
            */

            /*if(!isShadowed)*/ {
                // TODO: proper lighting model

                float lightFactor = 1.0f;
                if(light.type == 0) {
                    const float maxDist = 3;
                    float distance = length(light.position-worldPos);
                    lightFactor = max(0, (maxDist-distance)/maxDist); // TODO: calculate based on intensity, falloff, etc.
                }
                lightFactor *= max(0, dot(normal, lightDirection));
                lightContribution += light.color * lightFactor * light.intensity;
            }
        }
    }

    lightContribution = min(vec3(1.0)-lights.ambientColor, lightContribution);

    return lightContribution + lights.ambientColor;
}

void main() {
    vec4 outColorWorld;

    vec4 fragmentColor = texture(sampler2D(albedo, linearSampler), uv);
    vec3 worldPos = (cbo.inverseView * texture(sampler2D(viewPos, linearSampler), uv)).xyz;
    vec3 normal = normalize((cbo.inverseView * vec4(texture(sampler2D(viewNormals, linearSampler), uv).xyz, 0.0)).xyz);
    vec3 rtLighting = texture(sampler2D(rayTracedLighting, linearSampler), uv).rgb;
    vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;

    uint intProperties = texture(intPropertiesInput, uv).r;
    float currDepth = texture(sampler2D(depth, linearSampler), uv).r;
    if(currDepth < 1.0) {
        if((intProperties & IntPropertiesRayTracedLighting) == IntPropertiesRayTracedLighting) {
            outColorWorld = vec4(fragmentColor.rgb*rtLighting, fragmentColor.a);
        } else {
            outColorWorld = fragmentColor;
        }
        outColorWorld.rgb *= calculateLighting(worldPos, normal, false);
    } else {
        outColorWorld = vec4(skyboxRGB, fragmentColor.a);
    }

    vec4 transparentColor = texture(sampler2D(transparent, linearSampler), uv);

    vec3 blended = outColorWorld.rgb * (1.0 - transparentColor.a) + transparentColor.rgb * transparentColor.a;

    outColor = vec4(blended, 1.0);
}