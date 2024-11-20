#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define PREVIOUS_FRAME 0
#define CURRENT_FRAME 1

#ifndef HASH_GRID_SET_ID
#error HASH_GRID_SET_ID is not defined!
#endif
const uint InvalidCellIndex = 0xFFFFFFFFu;

layout(buffer_reference, scalar) buffer BufferToUints {
    uint v[];
};

struct HashCellKey {
    vec3 hitPosition;
    vec3 direction;
};

struct CellUpdate {
    HashCellKey key;
    uint pad; // wtf
    uint cellIndex;
    vec3 surfaceNormal;
    float metallic;
    float roughness;
};

struct HashDescriptor {
    uint bucketIndex;
    uint hash2;
};

struct HashCell {
    HashCellKey key;
    vec3 value;
    uint sampleCount;
    uint hash2;
};

layout(buffer_reference, scalar) buffer CellUpdateBuffer {
    CellUpdate v[];
};

layout(buffer_reference, scalar) buffer HashCellBuffer {
    HashCell v[];
};

layout(buffer_reference, scalar) buffer HashGrid {
    // update requests
    uint updateCount;
    uint maxUpdates;
    CellUpdateBuffer pUpdates; // must be as many as there are total cells inside the grid

    // write the frame number when each cell was last touched (used to decay cells)
    BufferToUints pLastTouchedFrame; // must be as many as there are total cells inside the grid

    HashCellBuffer pCells;
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

HashDescriptor computeDescriptor(in HashCellKey key) {
    HashDescriptor desc;
    float cellSize = 0.135f; // TODO
    ivec3 posQuantized = quantizePosition(key.hitPosition, cellSize);
    ivec2 dirQuantized = quantizeNormal(key.direction);
    desc.bucketIndex = pcg_hash(posQuantized.x + pcg_hash(posQuantized.y + pcg_hash(posQuantized.z + pcg_hash(dirQuantized.x + pcg_hash(dirQuantized.y))))) % bucketCount;
    desc.hash2 = xxhash32(posQuantized.x + xxhash32(posQuantized.y + xxhash32(posQuantized.z + xxhash32(dirQuantized.x + xxhash32(dirQuantized.y)))));
    return desc;
}

void hashGridClear(uint mapIndex, uint cellIndex) {
    HashCellKey nullKey;
    nullKey.hitPosition = vec3(0.0);
    nullKey.direction = vec3(0.0);
    grids[mapIndex].pCells.v[cellIndex].key = nullKey;
    grids[mapIndex].pCells.v[cellIndex].value = vec3(0.0);
    grids[mapIndex].pCells.v[cellIndex].hash2 = 0;
    grids[mapIndex].pCells.v[cellIndex].sampleCount = 0;
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
        uint previousHash = atomicCompSwap(grids[mapIndex].pCells.v[cellIndex].hash2, 0, desc.hash2);
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

    for(; cellIndex < boundary; cellIndex++) {
        if(grids[mapIndex].pCells.v[cellIndex].hash2 == desc.hash2) {
            break;
        }
    }
    if(cellIndex >= boundary) {
        return InvalidCellIndex; // not enough space :c
    }

    return cellIndex;
}

HashCellKey hashGridGetKey(uint mapIndex, uint cellIndex) {
    return grids[mapIndex].pCells.v[cellIndex].key;
}

uint hashGridReadSampleCount(uint mapIndex, uint cellIndex) {
    return grids[mapIndex].pCells.v[cellIndex].sampleCount;
}

vec3 hashGridRead(uint mapIndex, uint cellIndex) {
    return grids[mapIndex].pCells.v[cellIndex].value;
}

void hashGridWrite(uint mapIndex, uint cellIndex, in HashCellKey key, vec3 v, uint sampleCount) {
    grids[mapIndex].pCells.v[cellIndex].key = key;
    grids[mapIndex].pCells.v[cellIndex].hash2 = computeDescriptor(key).hash2;
    grids[mapIndex].pCells.v[cellIndex].sampleCount = sampleCount;
    grids[mapIndex].pCells.v[cellIndex].value = v;
}

/// Mark the tile as updated. If this returns true, the current invocation is the first one touching this cell for the current frame
bool hashGridMark(uint mapIndex, uint cellIndex, uint frameID) {
    uint previousFrameID = atomicExchange(grids[mapIndex].pLastTouchedFrame.v[cellIndex], frameID);
    return previousFrameID != frameID;
}

void hashGridMarkForUpdate(uint mapIndex, uint cellIndex, in HashCellKey key, vec3 surfaceNormal, float metallic, float roughness) {
    uint updateIndex = atomicAdd(grids[mapIndex].updateCount, 1);
    if(updateIndex < grids[mapIndex].maxUpdates) {
        grids[mapIndex].pUpdates.v[updateIndex].cellIndex = cellIndex;
        grids[mapIndex].pUpdates.v[updateIndex].key = key;
        grids[mapIndex].pUpdates.v[updateIndex].surfaceNormal = surfaceNormal;
        grids[mapIndex].pUpdates.v[updateIndex].metallic = metallic;
        grids[mapIndex].pUpdates.v[updateIndex].roughness = roughness;
    }
}

void hashGridResetCounters(uint mapIndex) {
    grids[mapIndex].updateCount = 0;
}