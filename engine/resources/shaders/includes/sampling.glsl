#extension GL_EXT_scalar_block_layout: require

struct Reservoir {
    vec3 bestSample;
    float weightSum;

    uint sampleCount;
    float resamplingWeight;
};

/**
 * Write a value to the given reservoir.
 * 'weight' is the weight of the value
 * 'u' is a random value between 0 and 1
 */
void writeToReservoir(inout Reservoir r, vec3 v, float weight, float u) {
    r.weightSum += weight;
    r.sampleCount++;
    if(u < weight / r.weightSum) {
        r.bestSample = v;
    }
}

vec3 readFromReservoir(in Reservoir r) {
    return r.bestSample * r.resamplingWeight;
}

void clearReservoir(inout Reservoir r) {
    r.weightSum = 0.0f;
    r.resamplingWeight = 0.0f;
    r.sampleCount = 0;
    r.bestSample = vec3(0);
}

Reservoir combineReservoirs(Reservoir a, Reservoir b, float u1, float u2) {
    Reservoir r;
    clearReservoir(r);

    #if 0
    r.sampleCount = a.sampleCount + b.sampleCount;
    r.weightSum = a.weightSum + b.weightSum;
    r.bestSample = (a.weightSum * a.bestSample + b.weightSum * b.bestSample) / (a.weightSum + b.weightSum);
    r.resamplingWeight = r.weightSum / r.sampleCount;
    #else
    writeToReservoir(r, a.bestSample, a.weightSum, u1);
    writeToReservoir(r, b.bestSample, b.weightSum, u2);

    r.sampleCount = a.sampleCount + b.sampleCount;
    r.resamplingWeight = r.weightSum / r.sampleCount;
    #endif
    return r;
}

vec4 averageSample3x3(texture2D toSample, sampler s, vec2 uv) {
    vec4 color = vec4(0.0);
    vec2 invTexSize = 1.0 / textureSize(toSample, 0);
    [[unroll]] for(int dx = -1; dx <= 1; dx++) {
        [[unroll]] for(int dy = -1; dy <= 1; dy++) {
            color += texture(sampler2D(toSample, s), uv + invTexSize * vec2(dx, dy));
        }
    }
    return color / 9;
}