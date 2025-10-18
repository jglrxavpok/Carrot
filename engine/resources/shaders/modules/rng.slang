module rng;
import modules.materials;
import modules.base;

// RNG based on blue noise textures.
// Calling a 'nextX' function will mutate the internal state of this RNG.
//  Therefore calling next() twice will yield different results
public struct RandomSampler {
    private uint pixelPos;
    private uint frameCount;
    private uint sampleIndex;

    public __init(float2 uv, uint frameWidth, uint frameHeight, uint frameCountIn) {
        const float2 screenSize = float2(frameWidth, frameHeight);
        uint2 pos = uint2(uv * screenSize);
        pixelPos = pos.x + pos.y * frameWidth;
        frameCount = frameCountIn;
        sampleIndex = 0;
    }

    [mutating]
    public float next(in MaterialSystem materialSystem) {
        const uint BLUE_NOISE_SIZE = 64;
        const uint NOISE_COUNT = 64;
        const float2 blueNoiseUV = r2Sequence(pixelPos);

        const uint components = 4;
        const uint timeCoordinate = sampleIndex + frameCount;
        const uint coordinate = timeCoordinate % components;
        const uint blueNoiseIndex = (timeCoordinate / components) % NOISE_COUNT;
        const uint textureIndex = materialSystem.globalTextures.blueNoises[blueNoiseIndex];
        float r = materialSystem.textures[textureIndex].SampleGrad(materialSystem.nearestSampler, blueNoiseUV, float2(0), float2(0))[coordinate];
        sampleIndex++;
        return r;
    }

    [mutating]
    public float2 nextConcentricDisk(in MaterialSystem materialSystem) {
        float2 u = float2(next(materialSystem), next(materialSystem)) * 2.0 - 1.0;
        if(u.x == 0 && u.y == 0)
        return float2(0.0);

        float theta = 0.0f;
        float r = 0.0f;

        if(abs(u.x) >= abs(u.y)) {
            r = u.x;
            theta = M_PI_OVER_4f * (u.y / u.x);
        } else {
            r = u.y;
            theta = M_PI_OVER_2f - M_PI_OVER_4f * (u.x / u.y);
        }
        return r * float2(cos(theta), sin(theta));
    }

    [mutating]
    public float3 nextCosineHemisphere(in MaterialSystem materialSystem) {
        float2 d = nextConcentricDisk(materialSystem);
        float z = sqrt(max(0, 1 - dot(d, d)));
        return float3(d.x, d.y, z);
    }
}

// from http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
public float2 r2Sequence(uint n) {
    const float g = 1.32471795724474602596f;
    const float a1 = 1.0f / g;
    const float a2 = 1.0f / (g*g);
    return fract(float2((0.5f+a1*n), (0.5f+a2*n)));
}