struct RandomSampler {
    uvec2 pixelPos;
    uint frameCount;
    uint sampleIndex;
};

void initRNG(inout RandomSampler rng, vec2 uv, uint frameWidth, uint frameHeight, uint frameCount) {
    const vec2 screenSize = vec2(frameWidth, frameHeight);

    rng.pixelPos = uvec2(uv * screenSize);
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
    const uvec2 noisePosition = rng.pixelPos + uvec2(r2Sequence(rng.sampleIndex) * uvec2(BLUE_NOISE_SIZE));
    const vec2 blueNoiseUV = vec2(noisePosition % BLUE_NOISE_SIZE) / BLUE_NOISE_SIZE;
    rng.sampleIndex++;

    const uint components = 4;
    const uint coordinate = rng.frameCount % components;
    const uint blueNoiseIndex = (rng.frameCount / components) % NOISE_COUNT;
    const uint textureIndex = globalTextures.blueNoises[blueNoiseIndex];
    float r = texture(sampler2D(textures[textureIndex], nearestSampler), blueNoiseUV)[coordinate];
    return r;
}