module gi;

import glsl;
import modules.base;
import modules.hash;
import modules.rng;
import modules.sh;
import modules.materials;

/**
* Grid containing the radiance value for a given location & direction
*/
namespace HashGrid {
    // keep in sync with LightingPasses.cpp
    public static const uint CellsPerBucket = 32;
    public static const uint BucketCount = 1024*256;
    public static const uint TotalCellCount = BucketCount * CellsPerBucket;

    // loosely based on https://gpuopen.com/download/publications/SA2021_WorldSpace_ReSTIR.pdf
    static const float cellSizeFactor = 0.1f;
    static const float minCellSize = 0.15f;

    uint giGetCellSizeStep(float3 hitPosition, float3 cameraPosition) {
        float cellSizeStep = distance(hitPosition, cameraPosition) * cellSizeFactor;
        return 0;//uint(floor(log2(cellSizeStep / minCellSize )));
    }

    public float giGetCellSize(float3 hitPosition, float3 cameraPosition) {
        return minCellSize * exp2(giGetCellSizeStep(hitPosition, cameraPosition));
    }

    int3 quantizePosition(float3 pos, float cellSize) {
        return int3(pos / cellSize);
    }

    float3 dequantizePosition(int3 posQuantized, float cellSize) {
        return vec3(posQuantized) * cellSize;
    }

    static const float thetaQuantum = 0.01f;
    static const float phiQuantum = 0.01f;

    uint3 quantizeNormal(float3 normal) {
        return asuint(int3(floor((normal * 0.5f + 0.5f) * 32 +0.5f)));
    }

    public struct Key {
        public float3 hitPosition = float3(0.0f);
        public float3 direction = float3(0.0f);
        public float3 cameraPosition = float3(0.0f);
        public float rayLength = 0.0f;

        public Descriptor toDescriptor() {
            Descriptor desc;
            #if 0
            uint cellSizeStep = giGetCellSizeStep(hitPosition, cameraPosition);
            float cellSize = minCellSize * exp2(cellSizeStep);
#else
            uint cellSizeStep = 0;
            float cellSize = minCellSize;
#endif
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
        Key* keys;//[TotalCellCount];
        uint* hash2;//[TotalCellCount];
        int3* radiance;//[TotalCellCount];
        uint* sampleCounts;//[TotalCellCount];
        int3* radianceAccumulation;//[TotalCellCount];
        uint* sampleCountAccumulation;//[TotalCellCount];

        // Used to decay cells which have not been used recently
        uint* lastTouchedFrames;//[TotalCellCount];

        [ForceInline]
        public bool isOutdated(uint cellIndex, uint currentFrame) {
            const uint decayTime = 20;
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
        // TODO: remove Key, not useful
        public void accumulate(uint cellIndex, in Key key, float3 radianceToAdd) {
            const int3 quantizedRadiance = packRadiance(radianceToAdd);
            InterlockedAdd(radianceAccumulation[cellIndex].x, quantizedRadiance.x);
            InterlockedAdd(radianceAccumulation[cellIndex].y, quantizedRadiance.y);
            InterlockedAdd(radianceAccumulation[cellIndex].z, quantizedRadiance.z);
            InterlockedAdd(sampleCountAccumulation[cellIndex], 1);
        }

        [mutating]
        public void accumulate2(uint cellIndex, float3 radianceToAdd) {
            const int3 quantizedRadiance = packRadiance(radianceToAdd);
            InterlockedAdd(radianceAccumulation[cellIndex].x, quantizedRadiance.x);
            InterlockedAdd(radianceAccumulation[cellIndex].y, quantizedRadiance.y);
            InterlockedAdd(radianceAccumulation[cellIndex].z, quantizedRadiance.z);
            InterlockedAdd(sampleCountAccumulation[cellIndex], 1);
        }

        [mutating]
        public float3 readRadiance(uint cellIndex) {
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
                // TODO: don't return, no samples mean no light, not that we did not try
                return; // nothing to do
            }
            float3 newRadiance = unpackRadiance(radianceAccumulation[cellIndex]) / newSampleCount;

            // Temporal accumulation, based on GI 1.0 implementation
            constexpr uint maxSampleCount = 64;
            uint totalSampleCount = min(maxSampleCount, newSampleCount + oldSampleCount);
            float3 finalRadiance;
            if(oldSampleCount == 0) {
                finalRadiance = newRadiance;
            } else {
                float3 oldRadiance = readRadiance(cellIndex);
                finalRadiance = lerp(oldRadiance, newRadiance, 1.0f / totalSampleCount);
            }
            writeRadiance(cellIndex, finalRadiance * totalSampleCount, totalSampleCount);

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

        public void jitter(inout RandomSampler rng, in MaterialSystem ms, inout GIInputs giInputs) {
            float3 tangent;
            float3 bitangent;
            computeTangents(giInputs.surfaceNormal, tangent, bitangent);
            const float jitterU = rng.next(ms) * 2 - 1;
            const float jitterV = rng.next(ms) * 2 - 1;

            float cellSize = giGetCellSize(giInputs.hitPosition, giInputs.cameraPosition);
            const vec3 jitterAmount = (tangent * jitterU + bitangent * jitterV) * cellSize;
            giInputs.hitPosition += jitterAmount;
        }

        public Key giInputsToKey(in GIInputs giInputs) {
            Key key;

            key.cameraPosition = giInputs.cameraPosition;
            key.hitPosition = giInputs.hitPosition;

            const float3 dRay = giInputs.hitPosition - giInputs.startOfRay;
            key.rayLength = length(dRay);
            key.direction = giInputs.surfaceNormal; // dRay / key.rayLength;
            return key;
        }

        // TODO: remove unused 'rng' parameter
        [mutating] // Mutating: Slang compiler has a bug where non-mutating member functions generate a copy of 'this'...
        public float3 read(inout RandomSampler rng, in GIInputs giInputs) {
            Key key = giInputsToKey(giInputs);

            if (let cellIndex = find(key)) {
                const uint sampleCount = sampleCounts[cellIndex];
                if(sampleCount == 0) {
                    return float3(0.0);
                }

                return readRadiance(cellIndex);
            }
            return float3(0, 0, 0);
        }
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
