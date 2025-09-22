#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include <includes/sampling.glsl>

#define PREVIOUS_FRAME 0
#define CURRENT_FRAME 1

#define MAX_RESERVOIRS 10

#ifndef HASH_GRID_SET_ID
#error HASH_GRID_SET_ID is not defined!
#endif
const uint InvalidCellIndex = 0xFFFFFFFFu;

// keep in sync with LightingPasses.cpp
const uint HashGridCellsPerBucket = 8;
const uint HashGridBucketCount = 1024*256;
const uint HashGridTotalCellCount = HashGridBucketCount*HashGridCellsPerBucket;

layout(buffer_reference, scalar) buffer BufferToUints {
    uint v[];
};

struct HashCellKey {
    vec3 hitPosition;
    vec3 direction;
    vec3 cameraPos;
    float rayLength;
};

struct HashDescriptor {
    uint bucketIndex;
    uint hash2;
};

layout(buffer_reference, scalar) buffer HashGrid {
    HashCellKey keys[HashGridTotalCellCount];
    uint hash2[HashGridTotalCellCount];
    ivec3 values[HashGridTotalCellCount];
    uint sampleCounts[HashGridTotalCellCount];

    uint lastTouchedFrames[HashGridTotalCellCount];
};

layout(set = HASH_GRID_SET_ID, binding = 0, scalar) buffer HashGridConstants {
    uint cellsPerBucket;
    uint bucketCount;
};

layout(set = HASH_GRID_SET_ID, binding = 1, scalar) buffer HashGridData {
    HashGrid grids[2];
};

// From https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint i)
{
    uint state = i * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// From Hash Functions for GPU Rendering: https://www.shadertoy.com/view/XlGcRh
// Which is based on:
// From Quality hashes collection (https://www.shadertoy.com/view/Xt3cDn)
// by nimitz 2018 (twitter: @stormoid)
// The MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
uint xxhash32(uint p)
{
    const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
    const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
    uint h32 = p + PRIME32_5;
    h32 = PRIME32_4*((h32 << 17) | (h32 >> (32 - 17)));
    h32 = PRIME32_2*(h32^(h32 >> 15));
    h32 = PRIME32_3*(h32^(h32 >> 13));
    return h32^(h32 >> 16);
}

const float radianceQuantum = 1.0f / 16384.0f;
const float invRadianceQuantum = 1.0f / radianceQuantum;

ivec3 packRadiance(vec3 r) {
    return ivec3(min(r, vec3(invRadianceQuantum)) * invRadianceQuantum);
}

vec3 unpackRadiance(ivec3 r) {
    return vec3(r) * radianceQuantum;
}

uint getBucketIndex(uint cellIndex) {
    return cellIndex / bucketCount;
}

ivec3 quantizePosition(vec3 pos, float cellSize) {
    return ivec3(pos / cellSize);
}

vec3 dequantizePosition(ivec3 posQuantized, float cellSize) {
    return vec3(posQuantized) * cellSize;
}

const float thetaQuantum = 0.05f;
const float phiQuantum = 0.05f;

ivec2 quantizeNormal(vec3 normal) {
    // assumed already normalized
    float theta = acos(normal.z);
    float phi = atan(normal.y, normal.x);
    return ivec2(theta / thetaQuantum, phi / phiQuantum);
}

vec3 dequantizeNormal(ivec2 normalQuantized) {
    float phi = normalQuantized.y * phiQuantum;
    return vec3(cos(phi), sin(phi), 0) * cos(normalQuantized.x * thetaQuantum);
}

// loosely based on https://gpuopen.com/download/publications/SA2021_WorldSpace_ReSTIR.pdf
const float cellSizeFactor = 32.0f;
const float minCellSize = 0.005f;

uint giGetCellSizeStep(vec3 hitPosition, vec3 cameraPosition) {
    float cellSizeStep = distance(hitPosition, cameraPosition) * cellSizeFactor;
    return uint(floor(log2(cellSizeStep / minCellSize )));
}

float giGetCellSize(vec3 hitPosition, vec3 cameraPosition) {
    return minCellSize * giGetCellSizeStep(hitPosition, cameraPosition);
}

HashDescriptor computeDescriptor(in HashCellKey key) {
    HashDescriptor desc;
    uint cellSizeStep = giGetCellSizeStep(key.hitPosition, key.cameraPos);
    float cellSize = minCellSize * cellSizeStep;
    ivec3 posQuantized = quantizePosition(key.hitPosition, cellSize);
    ivec2 dirQuantized = quantizeNormal(key.direction);
    int lightLeakFix = key.rayLength < cellSize ? 1 : 0;
    desc.bucketIndex = pcg_hash(lightLeakFix + pcg_hash(cellSizeStep + pcg_hash(posQuantized.x + pcg_hash(posQuantized.y + pcg_hash(posQuantized.z + pcg_hash(dirQuantized.x + pcg_hash(dirQuantized.y))))))) % bucketCount;
    desc.hash2 = xxhash32(lightLeakFix + xxhash32(cellSizeStep + xxhash32(posQuantized.x + xxhash32(posQuantized.y + xxhash32(posQuantized.z + xxhash32(dirQuantized.x + xxhash32(dirQuantized.y)))))));
    return desc;
}

void hashGridClear(in HashGrid pCells, uint cellIndex) {
    HashCellKey nullKey;
    nullKey.hitPosition = vec3(0.0);
    nullKey.direction = vec3(0.0);
    nullKey.cameraPos = vec3(0.0);
    nullKey.rayLength = 0.0;


    pCells.keys[cellIndex] = nullKey;
    pCells.hash2[cellIndex] = 0;

    pCells.sampleCounts[cellIndex] = 0;
    pCells.values[cellIndex] = packRadiance(vec3(0));
}

void hashGridClear(uint mapIndex, uint cellIndex) {
    hashGridClear(grids[mapIndex], cellIndex);
}

/// Inserts a tile inside the hash grid, returning the corresponding cell ID
/// 'wasNew' is used to know whether the cell already existed (false) or not
/// The hash grid *may* be full, and the returned cell ID will be 0
uint hashGridInsert(uint mapIndex, in HashCellKey key, out bool wasNew) {
    wasNew = false;
    HashDescriptor desc = computeDescriptor(key);
    uint cellIndex = desc.bucketIndex * cellsPerBucket;
    uint boundary = (desc.bucketIndex+1) * cellsPerBucket;

    // check if bucket already contains the cell
    for(; cellIndex < boundary; cellIndex++) {
        // either gets the cell, or adds it to the bucket
        uint previousHash = atomicCompSwap(grids[mapIndex].hash2[cellIndex], 0, desc.hash2);
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
        return InvalidCellIndex; // not enough space :c
    }

    return cellIndex;
}

uint hashGridFind(uint mapIndex, in HashCellKey key) {
    HashDescriptor desc = computeDescriptor(key);
    uint cellIndex = desc.bucketIndex * cellsPerBucket;
    uint boundary = (desc.bucketIndex+1) * cellsPerBucket;

    HashGrid pCells = grids[mapIndex];
    for(; cellIndex < boundary; cellIndex++) {
        if(pCells.hash2[cellIndex] == desc.hash2) {
            break;
        }
    }
    if(cellIndex >= boundary) {
        return InvalidCellIndex; // not enough space :c
    }

    return cellIndex;
}

HashCellKey hashGridGetKey(uint mapIndex, uint cellIndex) {
    return grids[mapIndex].keys[cellIndex];
}

vec3 hashGridRead(uint mapIndex, uint cellIndex) {
    return unpackRadiance(grids[mapIndex].values[cellIndex]);
}

uint hashGridGetSampleCount(uint mapIndex, uint cellIndex) {
    return grids[mapIndex].sampleCounts[cellIndex];
}

void hashGridAdd(uint mapIndex, uint cellIndex, in HashCellKey key, vec3 toAdd, uint newSampleCount) {
    // TODO: version without rewriting key & hash
    grids[mapIndex].keys[cellIndex] = key;
    grids[mapIndex].hash2[cellIndex] = computeDescriptor(key).hash2;

    ivec3 packedRadiance = packRadiance(toAdd);
    atomicAdd(grids[mapIndex].values[cellIndex].x, packedRadiance.x);
    atomicAdd(grids[mapIndex].values[cellIndex].y, packedRadiance.y);
    atomicAdd(grids[mapIndex].values[cellIndex].z, packedRadiance.z);

    atomicAdd(grids[mapIndex].sampleCounts[cellIndex], newSampleCount);
}

/**
 * Write to a cell of the hash grid
 * Each cell has multiple reservoirs, so the reservoirIndex needs to be specified.
 * Furthermore, to avoid a dependency on rng, this method requires a random value (u) to be provided
 */
void hashGridWrite(uint mapIndex, uint cellIndex, in HashCellKey key, vec3 value, uint sampleCount) {
    // TODO: version without rewriting key & hash
    grids[mapIndex].keys[cellIndex] = key;
    grids[mapIndex].hash2[cellIndex] = computeDescriptor(key).hash2;
    grids[mapIndex].values[cellIndex] = packRadiance(value);
    grids[mapIndex].sampleCounts[cellIndex] = sampleCount;
}

/// Mark the tile as updated. If this returns true, the current invocation is the first one touching this cell for the current frame
bool hashGridMark(uint mapIndex, uint cellIndex, uint frameID) {
    uint previousFrameID = atomicExchange(grids[mapIndex].lastTouchedFrames[cellIndex], frameID);
    return previousFrameID != frameID;
}