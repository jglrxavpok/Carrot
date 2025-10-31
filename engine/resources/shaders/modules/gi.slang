module gi;

import modules.base;
import modules.hash;
import modules.rng;
import modules.sh;


/**
* Grid containing the radiance value for a given location & direction
*/
namespace HashGrid {
    // keep in sync with LightingPasses.cpp
    public static const uint CellsPerBucket = 16;
    public static const uint BucketCount = 1024*4;
    public static const uint TotalCellCount = BucketCount * CellsPerBucket;

    // loosely based on https://gpuopen.com/download/publications/SA2021_WorldSpace_ReSTIR.pdf
    static const float cellSizeFactor = 32.0f;
    static const float minCellSize = 0.1f;

    uint giGetCellSizeStep(float3 hitPosition, float3 cameraPosition) {
        float cellSizeStep = distance(hitPosition, cameraPosition) * cellSizeFactor;
        return uint(floor(log2(cellSizeStep / minCellSize )));
    }

    float giGetCellSize(float3 hitPosition, float3 cameraPosition) {
        return minCellSize * exp2(giGetCellSizeStep(hitPosition, cameraPosition));
    }

    int3 quantizePosition(float3 pos, float cellSize) {
        return int3(pos / cellSize);
    }

    float3 dequantizePosition(int3 posQuantized, float cellSize) {
        return vec3(posQuantized) * cellSize;
    }

    static const float thetaQuantum = 0.05f;
    static const float phiQuantum = 0.05f;

    uint3 quantizeNormal(float3 normal) {
        return asuint(int3(floor((normal * 0.5f + 0.5f) * 4 +0.5f)));
    }

    public struct Key {
        public float3 hitPosition = float3(0.0f);
        public float3 direction = float3(0.0f);
        public float3 cameraPosition = float3(0.0f);
        public float rayLength = 0.0f;

        public Descriptor toDescriptor() {
            Descriptor desc;
            uint cellSizeStep = giGetCellSizeStep(hitPosition, cameraPosition);
            float cellSize = minCellSize * exp2(cellSizeStep);
            int3 posQuantized = quantizePosition(hitPosition, cellSize);
            uint3 dirQuantized = quantizeNormal(direction);
            int lightLeakFix = rayLength < cellSize ? 1 : 0;
            desc.bucketIndex = pcg_hash(lightLeakFix 
                + pcg_hash(cellSizeStep 
                    + pcg_hash(posQuantized.x 
                        + pcg_hash(posQuantized.y 
                            + pcg_hash(posQuantized.z 
                                + pcg_hash(dirQuantized.x 
                                    + pcg_hash(dirQuantized.y
                                        + pcg_hash(dirQuantized.z)))))))) 
                                        % BucketCount;
            desc.hash2 = xxhash32(lightLeakFix 
                + xxhash32(cellSizeStep 
                    + xxhash32(posQuantized.x 
                        + xxhash32(posQuantized.y 
                            + xxhash32(posQuantized.z 
                                + xxhash32(dirQuantized.x 
                                    + xxhash32(dirQuantized.y
                                        + xxhash32(dirQuantized.z))))))));
            
            return desc;
        }
    }

    public struct Descriptor {
        public uint bucketIndex;
        public uint hash2;
    }

    public struct GIInputs {
        public float3 hitPosition;
        public float3 startOfRay;
        public float3 cameraPosition;
        public float3 surfaceNormal;
    }

    public struct Storage {
        Key keys[TotalCellCount];
        uint hash2[TotalCellCount];
        int3 radiance[TotalCellCount];
        uint sampleCounts[TotalCellCount];
        int3 radianceAccumulation[TotalCellCount];
        uint sampleCountAccumulation[TotalCellCount];

        // Used to decay cells which have not been used recently
        uint lastTouchedFrames[TotalCellCount];

        [ForceInline]
        public bool isOutdated(uint cellIndex, uint currentFrame) {
            const uint decayTime = 100;
            return lastTouchedFrames[cellIndex]+decayTime < currentFrame;
        }

        [mutating][ForceInline]
        public void clearCell(uint cellIndex) {
            keys[cellIndex] = Key();
            hash2[cellIndex] = 0;
            radiance[cellIndex] = packRadiance(0);
            sampleCounts[cellIndex] = 0;
        }

        /// Inserts a tile inside the hash grid, returning the corresponding cell ID
        /// 'wasNew' is used to know whether the cell already existed (false) or not
        /// The hash grid *may* be full, and the returned cell ID will be none
        [mutating]
        public Optional<uint> insert(in Key key, out bool wasNew) {
            wasNew = false;
            Descriptor desc = key.toDescriptor();
            uint cellIndex = desc.bucketIndex * CellsPerBucket;
            uint boundary = (desc.bucketIndex+1) * CellsPerBucket;
            // check if bucket already contains the cell
            for(; cellIndex < boundary; cellIndex++) {
                // either gets the cell, or adds it to the bucket
                uint previousHash;
                InterlockedCompareExchange(hash2[cellIndex], 0, desc.hash2, previousHash);
                if(previousHash == 0) {
                    wasNew = true;
                    //hashGridClear(mapIndex, cellIndex);
                    break;
                }
                if(previousHash == desc.hash2) {
                    break;
                }
            }
            if(cellIndex >= boundary) {
                return none; // not enough space :c
            }

            return cellIndex;
        }

        [mutating] // Mutating: Slang compiler has a bug where non-mutating member functions generate a copy of 'this'...
        public Optional<uint> find(in Key key) {
            const Descriptor desc = key.toDescriptor();
            uint cellIndex = desc.bucketIndex * CellsPerBucket;
            uint boundary = (desc.bucketIndex+1) * CellsPerBucket;

            for(; cellIndex < boundary; cellIndex++) {
                if(hash2[cellIndex] == desc.hash2) {
                    break;
                }
            }
            if(cellIndex >= boundary) {
                return none; // not enough space :c
            }

            return cellIndex;
        }

        [mutating]
        public void writeToCell(uint cellIndex, in Key key, float3 radianceValue, uint newSampleCount) {
            keys[cellIndex] = key;
            hash2[cellIndex] = key.toDescriptor().hash2;
            radiance[cellIndex] = packRadiance(radianceValue);
            sampleCounts[cellIndex] = newSampleCount;
        }

        [mutating]
        public void accumulate(uint cellIndex, in Key key, float3 radianceToAdd) {
            const int3 quantizedRadiance = packRadiance(radianceToAdd);
            InterlockedAdd(radianceAccumulation[cellIndex].x, quantizedRadiance.x);
            InterlockedAdd(radianceAccumulation[cellIndex].y, quantizedRadiance.y);
            InterlockedAdd(radianceAccumulation[cellIndex].z, quantizedRadiance.z);
            InterlockedAdd(sampleCountAccumulation[cellIndex], 1);
        }

        [mutating]
        private float3 readRadiance(uint cellIndex) {
            return unpackRadiance(radiance[cellIndex]) / sampleCounts[cellIndex];
        }

        [mutating]
        private void writeRadiance(uint cellIndex, float3 newRadiance, uint sampleCount) {
            radiance[cellIndex] = packRadiance(newRadiance);
            sampleCounts[cellIndex] = sampleCount;
        }

        [mutating]
        public void mergeRadiance(uint cellIndex) {
            const uint oldSampleCount = sampleCounts[cellIndex];
            const uint newSampleCount = sampleCountAccumulation[cellIndex];
            if(newSampleCount == 0) {
                return; // nothing to do
            }
            float3 oldRadiance = readRadiance(cellIndex);
            float3 newRadiance = unpackRadiance(radianceAccumulation[cellIndex]) / newSampleCount;

            if(oldSampleCount == 0) {
                writeRadiance(cellIndex, newRadiance, newSampleCount);
            } else {
                // Temporal accumulation, based on GI 1.0 implementation
                constexpr uint maxSampleCount = 16;
                uint totalSampleCount = min(maxSampleCount, newSampleCount + oldSampleCount);
                const float3 finalRadiance = lerp(oldRadiance, newRadiance, 1.0f / totalSampleCount);
                writeRadiance(cellIndex, finalRadiance * totalSampleCount, totalSampleCount);
            }

            // clear accumulation buffer
            radianceAccumulation[cellIndex] = 0;
            sampleCountAccumulation[cellIndex] = 0;
        }

        [mutating]
        public bool mark(uint cellIndex, uint frameID) {
            uint previousFrameID;
            InterlockedExchange(lastTouchedFrames[cellIndex], frameID, previousFrameID);
            return previousFrameID != frameID;
        }

        public float3 computeJitterAmount(inout RandomSampler rng, in GIInputs giInputs) {
            return float3(0.0); // TODO
        }

        [mutating] // Mutating: Slang compiler has a bug where non-mutating member functions generate a copy of 'this'...
        public float3 read(inout RandomSampler rng, in GIInputs giInputs) {
            Key cellDesc;

            const float3 jitter = computeJitterAmount(rng, giInputs);
            cellDesc.cameraPosition = giInputs.cameraPosition;
            cellDesc.hitPosition = giInputs.hitPosition + jitter;
            
            const float3 dRay = giInputs.hitPosition - giInputs.startOfRay;
            cellDesc.rayLength = length(dRay);
            cellDesc.direction = dRay / cellDesc.rayLength;

            if(let cellIndex = find(cellDesc)) {
                const uint sampleCount = sampleCounts[cellIndex];
                if(sampleCount == 0) {
                    return float3(0.0);
                }

                return readRadiance(cellIndex);
            }
            return float3(0,0,0);
        }
    }

    public struct Constants {
        public uint cellsPerBucket;
        public uint bucketCount;
    }

    public struct Grids {
        public Storage* previousFrame;
        public Storage* currentFrame;
    }

    public struct Block {
        public ConstantBuffer<Constants> constants;
        public GLSLShaderStorageBuffer<Grids, ScalarDataLayout> grids;
    }
}

public static const uint ScreenProbeSize = 8;

public struct RayData {
    public uint32_t probeIndex;
    public float3 radiance;
    public float3 direction;
}

struct SHRadiance {
    SH3 r;
    SH3 g;
    SH3 b;

    [mutating]
    void integrate(float3 value, float3 direction) {
        r.integrate(value.r, direction);
        g.integrate(value.g, direction);
        b.integrate(value.b, direction);
    }

    [mutating]
    void add(const in SHRadiance other) {
        r += other.r;
        g += other.g;
        b += other.b;
    }

    [mutating]
    void reset() {
        r.reset();
        g.reset();
        b.reset();
    }

    SHRadiance scale(float f) {
        SHRadiance result;
        result.r = r.scale(f);
        result.g = g.scale(f);
        result.b = b.scale(f);
        return result;
    }

    float3 evalutateCosineLobe(float3 direction) {
        return float3(
            r.evaluateCosineLobe(direction),
            g.evaluateCosineLobe(direction),
            b.evaluateCosineLobe(direction),
        );
    }
}

SHRadiance operator*(const in SHRadiance a, float f) {
    SHRadiance r = a;
    r.r *= f;
    r.g *= f;
    r.b *= f;
    return r;
}

SHRadiance operator/(const in SHRadiance a, float f) {
    SHRadiance r = a;
    r.r /= f;
    r.g /= f;
    r.b /= f;
    return r;
}

SHRadiance lerp(const in SHRadiance a, const in SHRadiance b, float t) {
    SHRadiance result = a.scale(1.0f - t);
    SHRadiance tmp = b.scale(t);
    result.add(tmp);
    return result;
}

public struct ScreenProbe {
    SHRadiance radiance;
    public float3 worldPos;
    public float3 normal;
    public int2 bestPixel;
    uint sampleCount;

    SHRadiance accumulatedRadiance;
    uint accumulatedSamples;

    public float3 readRadiance() {
        // TODO: vector to camera
        return radiance.evalutateCosineLobe(normal) / sampleCount;
    }

    [mutating]
    public void reset(float3 worldPos, float3 normal, int2 bestPixel) {
        this.worldPos = worldPos;
        this.normal = normal;
        this.radiance.reset();
        this.sampleCount = 0;
        this.bestPixel = bestPixel;

        this.accumulatedRadiance.reset();
        this.accumulatedSamples = 0;
    }

    [mutating]
    public void reprojectRadiance(in ScreenProbe other) {
        this.radiance = other.radiance;
        this.sampleCount = other.sampleCount;
        this.accumulatedRadiance.reset();
        this.accumulatedSamples = 0;
    }

    [mutating]
    public void accumulateSample(const in RayData ray) {
        if(dot(ray.radiance, ray.radiance) > 0.01f) {
            accumulatedRadiance.integrate(ray.radiance, ray.direction);
            accumulatedSamples++;
        }
    }

    [mutating]
    public void finishAccumulateSamples() {
        const uint oldSampleCount = sampleCount;
        const uint newSampleCount = accumulatedSamples;
        if(newSampleCount == 0) {
            return; // nothing to do
        }
        SHRadiance oldRadiance = radiance / oldSampleCount;
        SHRadiance newRadiance = accumulatedRadiance / newSampleCount;

        if(oldSampleCount == 0) {
            radiance = newRadiance;
            sampleCount = newSampleCount;
        } else {
            // Temporal accumulation, based on GI 1.0 implementation
            constexpr uint maxSampleCount = 128;
            uint totalSampleCount = min(maxSampleCount, newSampleCount + oldSampleCount);
            radiance = lerp(oldRadiance, newRadiance, 1.0f / totalSampleCount);
            radiance = radiance.scale(totalSampleCount);
            sampleCount = totalSampleCount;
        }

        // clear accumulation buffer
        accumulatedRadiance.reset();
        accumulatedSamples = 0;
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

// position, in screen space of the pixel that will spawn the rays used to fill the GI grid
public struct SpawnedRay { 
    public uint probeIndex;
};

static const float radianceQuantum = 1.0f / 16384.0f;
static const float invRadianceQuantum = 1.0f / radianceQuantum;

int3 packRadiance(float3 r) {
    return int3(min(r, float3(invRadianceQuantum)) * invRadianceQuantum);
}

float3 unpackRadiance(int3 r) {
    return float3(r) * radianceQuantum;
}
