import modules.base;
import modules.virtual_geometry;
import modules.viewport;
import modules.camera;
import modules.vertex_buffers;

static const constexpr int TaskWorkGroupSize = 32;
static const constexpr int MeshWorkGroupSize = 64;
static const constexpr int MaxVertices = 64;
static const constexpr int MaxTriangles = 128;

struct Statistics {
    uint totalTriangleCount;
}

struct IO {
    StructuredBuffer<Cluster, ScalarDataLayout> clusters;
    StructuredBuffer<ClusterInstance, ScalarDataLayout> clusterInstances;
    StructuredBuffer<ClusterBasedModelData, ScalarDataLayout> modelData;
    RWTexture2D<uint64_t> outputImage;

    RWStructuredBuffer<Statistics, ScalarDataLayout> stats;
    RWStructuredBuffer<uint, ScalarDataLayout> activeClusters;
};

ParameterBlock<IO> io;
ParameterBlock<Camera> cameras;
ParameterBlock<Viewport> viewport;

enum LodSelectionMode {
    ScreenSize,
    Forced,
}

[Flags]
enum ConfigurationFlags {
    None = 0,
    OutputTriangleCount
}

struct PushConstant {
    uint maxCluster;
    LodSelectionMode lodSelection;
    float lodErrorThreshold;
    uint forcedLOD;

    ConfigurationFlags flags;
}


// Task -> Mesh communication
struct Payload {
    uint clusterInstanceIDs[TaskWorkGroupSize];
}

// Mesh -> Fragment shader communication
struct PixelIn {
    float4 pos: SV_Position;
    float4 ndcPosition;
    nointerpolation uint instanceID;
}

struct PrimitiveAttributes {
    uint primitiveID : SV_PrimitiveID;
}

bool cull(const in ClusterInstance clusterInstance, Optional<uint> forcedLOD, float errorThreshold) {
    const Cluster cluster = io.clusters[clusterInstance.clusterID];
    const ClusterBasedModelData clusterInstanceData = io.modelData[clusterInstance.instanceDataIndex];
    if(clusterInstanceData.visible == 0) {
        return true;
    }

    if(let forcedLODValue = forcedLOD) {
        return cluster.getLOD() != forcedLODValue;
    } else { // based on screen size
        const float4x4 modelMatrix = clusterInstanceData.instanceData.transform * transpose(float4x4(transpose(cluster.transform), float4(0,0,0,1)));
        const float3 cameraPos = (cameras.CurrentFrame().view * float4(0,0,0,1)).xyz;
        
        // frustum check
        const float4 worldSphere = transformSphere(cluster.getBoundingSphere(), modelMatrix);
        if(!cameras.CurrentFrame().frustumCheck(worldSphere.xyz, worldSphere.w)) {
            return true;
        }

        // check if this cluster is not too simple, but not too detailed either
        const float selfError = viewport.frameHeight * cluster.computeScreenError(cameraPos, modelMatrix);
        const float refinedError = viewport.frameHeight * cluster.computeScreenErrorOfRefinedOrigin(cameraPos, modelMatrix);
        const bool render = selfError > errorThreshold && refinedError <= errorThreshold;
        return !render;
    }
}

groupshared uint meshletCount;
groupshared Payload payload;
[numthreads(TaskWorkGroupSize, 1, 1)]
[shader("amplification")]
void task(uniform PushConstant push, 
    uint localInvocationIndex: SV_GroupIndex, 
    uint workgroupID: SV_GroupID) {

    meshletCount = 0;
    GroupMemoryBarrier();

    uint clusterID = io.activeClusters[localInvocationIndex + workgroupID * TaskWorkGroupSize];
    bool shouldRender = clusterID < push.maxCluster;

    if(shouldRender) {
        const bool culled = cull(
            io.clusterInstances[clusterID], 
            select(push.lodSelection == LodSelectionMode::Forced, push.forcedLOD, none),
            push.lodErrorThreshold
        );

        if(!culled) {
            uint index;
            InterlockedAdd(meshletCount, 1, index);
            payload.clusterInstanceIDs[index] = clusterID;
        }
    }

    GroupMemoryBarrier();
    // Values are taken from arguments provided by the first invocation, it actually is undefined behavior to call EmitMeshTasksEXT in a non-uniform control flow
    DispatchMesh(meshletCount, 1, 1, payload);
}

[numthreads(MeshWorkGroupSize, 1, 1)]
[outputtopology("triangle")]
[shader("mesh")]
void mesh(const in uint workgroupID : SV_GroupID,
    const in uint localIndex : SV_GroupIndex,
    uniform PushConstant push,
    const in payload Payload payload,
    out vertices PixelIn outVertices[MaxVertices],
    out indices uint3 outIndices[MaxTriangles],
    out primitives PrimitiveAttributes outPrimitives[MaxTriangles]) {
    
    const uint instanceID = payload.clusterInstanceIDs[workgroupID % TaskWorkGroupSize];
    const ClusterInstance clusterInstance = io.clusterInstances[instanceID];
    const uint clusterID = clusterInstance.clusterID;
    const Cluster cluster = io.clusters[clusterID];
    const uint modelDataIndex = clusterInstance.instanceDataIndex;

    // cluster.transform is an OpenGL mat4x3, but that's a float3x4 in Slang, so we need this dance to make it a float4x4
    const float4x4 modelview = cameras.CurrentFrame().view * io.modelData[modelDataIndex].instanceData.transform * transpose(float4x4(transpose(cluster.transform), float4(0,0,0,1)));

    SetMeshOutputCounts(uint(cluster.vertexCount), uint(cluster.triangleCount));

    const float4x4 viewProj = cameras.CurrentFrame().jitteredProjection * modelview;

    // all threads of workgroup will contribute to filling output variables
    for(uint vertexIndex = localIndex; vertexIndex < cluster.vertexCount; vertexIndex += MeshWorkGroupSize) {
        outVertices[vertexIndex].ndcPosition = viewProj * cluster.vertices[vertexIndex].getPos();
        outVertices[vertexIndex].pos = outVertices[vertexIndex].ndcPosition;
        outVertices[vertexIndex].instanceID = instanceID;
    }

    for(uint triangleIndex = localIndex; triangleIndex < cluster.triangleCount; triangleIndex += MeshWorkGroupSize) {
        uint3 indices = uint3(
            cluster.indices[triangleIndex * 3 + 0],
            cluster.indices[triangleIndex * 3 + 1],
            cluster.indices[triangleIndex * 3 + 2]);
        outIndices[triangleIndex] = indices;
        outPrimitives[triangleIndex].primitiveID = triangleIndex;
    }

    if(localIndex == 0 && (push.flags & ConfigurationFlags::OutputTriangleCount)) {
        InterlockedAdd(io.stats[0].totalTriangleCount, cluster.triangleCount);
    }
}

[shader("pixel")]
void pixel(in PixelIn input, uint primitiveID: SV_PrimitiveID) {
    int w, h;
    io.outputImage.GetDimensions(w, h);
    int2 imageSize = int2(w, h);

    float4 ndc = input.ndcPosition;
    if(ndc.z < 0) {
        discard;
    }

    ndc /= ndc.w;
    const float2 pixelCoordsFloat = (ndc.xy + 1) / 2 * imageSize;
    const int2 pixelCoods = int2(pixelCoordsFloat);
    const uint depth = 0xFFFF_FFFFu - uint(double(ndc.z) * 0xFFFF_FFFFu);
    // 32 high bits: depth
    // 32 low bits: 25 high bits instance ID, 7 remaining: triangle index
    const uint low = ((input.instanceID & 0x1FF_FFFFu) << 7) | (uint(primitiveID) & 0x7Fu);
    uint64_t value = uint64_t(low) | (uint64_t(depth) << 32ul);

    InterlockedMax(io.outputImage[pixelCoods], value);
}