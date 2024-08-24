#include <lighting/base.common.glsl>

vec3 calculateDirectLighting(inout RandomSampler rng, vec3 worldPos, vec3 emissive, vec3 normal, vec3 tangent, vec2 metallicRoughness, bool raytracing) {
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
        vec3 lightAtPoint = computeDirectLightingFromLights(/*inout*/rng, /*inout*/lightPDF, rayOrigin, normal, MAX_LIGHT_DISTANCE);
        return lightAtPoint * lightPDF;
    }
    #endif
}
