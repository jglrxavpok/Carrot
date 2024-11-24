#ifndef _RNG_GLSL
#define _RNG_GLSL
#include <includes/math.glsl>

#ifndef MATERIAL_SYSTEM_SET
#error rng.glsl requires materials (for blue noise textures)
#endif

struct RandomSampler {
    uint pixelPos;
    uint frameCount;
    uint sampleIndex;
};

void initRNG(inout RandomSampler rng, vec2 uv, uint frameWidth, uint frameHeight, uint frameCount) {
    const vec2 screenSize = vec2(frameWidth, frameHeight);

    uvec2 pos = uvec2(uv * screenSize);
    rng.pixelPos = pos.x + pos.y * frameWidth;
    rng.frameCount = frameCount;
    rng.sampleIndex = 0;
}

// from http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
vec2 r2Sequence(uint n) {
    const float g = 1.32471795724474602596f;
    const float a1 = 1.0f / g;
    const float a2 = 1.0f / (g*g);
    return fract(vec2((0.5f+a1*n), (0.5f+a2*n)));
}

float sampleNoise(inout RandomSampler rng) {
    const uint BLUE_NOISE_SIZE = 64;
    const uint NOISE_COUNT = 64;
    const vec2 blueNoiseUV = r2Sequence(rng.pixelPos);

    const uint components = 4;
    const uint timeCoordinate = rng.sampleIndex + rng.frameCount;
    const uint coordinate = timeCoordinate % components;
    const uint blueNoiseIndex = (timeCoordinate / components) % NOISE_COUNT;
    const uint textureIndex = globalTextures.blueNoises[blueNoiseIndex];
    float r = texture(sampler2D(textures[textureIndex], nearestSampler), blueNoiseUV)[coordinate];
    rng.sampleIndex++;
    return r;
}

vec2 concentricSampleDisk(inout RandomSampler rng) {
    vec2 u = vec2(sampleNoise(rng), sampleNoise(rng)) * 2.0 - 1.0;
    if(u.x == 0 && u.y == 0)
    return vec2(0.0);

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
    float z = sqrt(max(0, 1 - dot(d, d)));
    return vec3(d.x, d.y, z);
}

#endif // _RNG_GLSL