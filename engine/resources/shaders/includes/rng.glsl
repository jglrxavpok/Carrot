struct RandomSampler {
    uint seed;
};

uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

// from https://www.mattkeeter.com/projects/rayray/ (MIT)
// ( https://github.com/mkeeter/rayray )
// Returns a pseudorandom value between -1 and 1
float rand(inout uint seed) {
    // 32-bit LCG Multiplier from
    // "Computationally Easy, Spectrally Good Multipliers for
    //  Congruential Pseudorandom Number Generators" [Steele + Vigna]
    seed = 0xadb4a92d * seed + 1;

    // Low bits have less randomness [L'ECUYER '99], so we'll shift the high
    // bits into the mantissa position of an IEEE float32, then mask with
    // the bit-pattern for 2.0
    uint m = (seed >> 9) | 0x40000000u;

    float f = uintBitsToFloat(m);   // Range [2:4]
    return f - 3.0;                 // Range [-1:1]
}

void initRNG(inout RandomSampler rng, vec2 uv, uint frameWidth, uint frameHeight, uint frameCount) {
    const vec2 screenSize = vec2(frameWidth, frameHeight);

    const vec2 pixelPos = uv * screenSize;
    const uint pixelIndex = uint(pixelPos.x + pixelPos.y * frameWidth);
    rng.seed = wang_hash(wang_hash(pixelIndex) ^ frameCount);
}

float sampleNoise(inout RandomSampler rng) {
    return (rand(rng.seed)+1.0)/2.0;
}