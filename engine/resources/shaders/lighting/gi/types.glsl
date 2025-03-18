//
// Created by jglrxavpok on 15/03/2025.
//

struct ScreenProbe {
    ivec3 radiance;
    vec3 worldPos;
    vec3 normal;
    ivec2 bestPixel;
    uint sampleCount;
};

// position, in screen space of the pixel that will spawn the rays used to fill the GI grid
struct SpawnedRay {
    uint probeIndex;
};