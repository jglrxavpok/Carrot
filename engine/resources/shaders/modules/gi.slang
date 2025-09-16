module gi;

import modules.base;

/**
* Grid containing the radiance value for a given location & direction
*/
namespace HashGrid {
    // keep in sync with LightingPasses.cpp
    public static const uint CellsPerBucket = 8;
    public static const uint BucketCount = 1024*256*4;
    public static const uint TotalCellCount = BucketCount * CellsPerBucket;

    public struct Key {
        public float3 hitPosition;
        public float3 direction;
        public float3 cameraPosition;
        public float rayLength;
    }

    public struct Descriptor {
        public uint bucketIndex;
        public uint hash2;
    }

    public struct Storage {
        public Key keys[TotalCellCount];
        public uint hash2[TotalCellCount];
        public int3 radiance[TotalCellCount];
        public uint sampleCounts[TotalCellCount];

        // Used to decay cells which have not been used recently
        public uint lastTouchedFrames[TotalCellCount];
    }

    public struct Constants {
        public uint cellsPerBucket;
        public uint bucketCount;
    }

    public struct Grids {
        public Storage grids[2]; // 0 = previous frame, 1 = current frame
    }

    public struct Block {
        public ConstantBuffer<Constants> constants;
        public ConstantBuffer<Grids> grids;
    }
}

public static const uint ScreenProbeSize = 8;

public struct ScreenProbe {
    int3 radiance;
    float3 worldPos;
    float3 normal;
    int2 bestPixel;
    uint sampleCount;

    public float3 readRadiance() {
        return unpackRadiance(radiance) / sampleCount;
    }
}

public struct ScreenProbeArray {
    public uint count;
    public uint[] indices;
}

public struct ScreenProbes {
    private StructuredBuffer<ScreenProbe, ScalarDataLayout> probes;

    /**
        Returns the screen probe covering the given screen position.
        Assumes all parameters are valid
    */
    public ScreenProbe getFromScreenPos(uint2 screenPos, uint frameWidth) {
        uint2 probePosition = screenPos / ScreenProbeSize;
        const uint probesPerWidth = (frameWidth+ScreenProbeSize-1) / ScreenProbeSize;
        uint probeIndex = probePosition.x + probePosition.y * probesPerWidth;
        return probes[NonUniformResourceIndex(probeIndex)];
    }
}

static const float radianceQuantum = 1.0f / 16384.0f;
static const float invRadianceQuantum = 1.0f / radianceQuantum;

int3 packRadiance(float3 r) {
    return int3(min(r, float3(invRadianceQuantum)) * invRadianceQuantum);
}

float3 unpackRadiance(int3 r) {
    return float3(r) * radianceQuantum;
}
