#include <lighting/base.common.glsl>

vec2 concentricSampleDisk(inout RandomSampler rng) {
    vec2 u = vec2(sampleNoise(rng), sampleNoise(rng)) * 2.0 - 1.0;
    if(u.x == 0 && u.y == 0) {
        return vec2(0.0);
    }

    float theta = 0.0f;
    float r = 0.0f;

    if(abs(u.x) >= abs(u.y)) {
        r = u.x;
        theta = M_PI_OVER_4 * (u.y / u.x);
    } else {
        r = u.y;
        theta = M_PI_OVER_2 - M_PI_OVER_4 * (u.x / u.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

vec3 cosineSampleHemisphere(inout RandomSampler rng) {
    vec2 d = concentricSampleDisk(rng);
    float z = sqrt(max(0, 1.0f - dot(d, d)));
    return vec3(d.x, d.y, z);
}

vec3 sphericalDirection(float sinTheta, float cosTheta, float phi) {
    return vec3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );
}

float calculateAO(inout RandomSampler rng, vec3 worldPos, vec3 emissive, vec3 normal, vec3 tangent, vec2 metallicRoughness, bool raytracing) {
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
        vec3 T = tangent;
        vec3 N = normal;
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(T, N);

        vec3 directionTangentSpace = cosineSampleHemisphere(rng);
        vec3 direction = T * directionTangentSpace.x + B * directionTangentSpace.y + N * directionTangentSpace.z;
        float tMin = 0.001f;
        float tMax = 0.10f; // 10cm
        initRayQuery(worldPos, direction, tMax, tMin);
        bool noIntersection = traceShadowRay();
        return noIntersection ? 1.0f : 0.0f;
    }
    #endif
}
