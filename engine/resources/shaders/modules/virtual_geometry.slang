import modules.base;
import modules.vertex_buffers;

public struct Cluster {
    public PackedVertex* vertices;
    public uint16_t* indices;
    public uint8_t triangleCount;
    public uint8_t vertexCount;
    uint8_t lod;
    public float4x3 transform;
    float4 boundingSphere;
    float4 refinedBoundingSphere;
    float error;
    float refinedError;

    uint pad; // alignment required due to usage of pointers

    private float computeScreenError(float3 cameraPosition, float4x4 modelMatrix, float4 boundingSphere, float clusterError) {
        // assume a fixed resolution and fov
        const float testFOV = M_PI_OVER_2f;
        const float cotHalfFov = 1.0f / tan(testFOV / 2.0f);

        float error = 0.0f;
        float4 transformedSphere = transformSphere(boundingSphere, modelMatrix);
        float3 dpos = transformedSphere.xyz - cameraPosition;
        float d = sqrt(dot(dpos, dpos)) - transformedSphere.w;
        error = clusterError / (d /*TODO: camera znear*/) * cotHalfFov;
        error = length((modelMatrix * float4(error, 0,0,0)).xyz);
        return error;
    }

    // Needs to be multiplied by screen height after (ratio in 0..1)
    public float computeScreenError(float3 cameraPosition, float4x4 modelMatrix) {
        return computeScreenError(cameraPosition, modelMatrix, boundingSphere, error);
    }

    // Needs to be multiplied by screen height after (ratio in 0..1)
    public float computeScreenErrorOfRefinedOrigin(float3 cameraPosition, float4x4 modelMatrix) {
        return computeScreenError(cameraPosition, modelMatrix, refinedBoundingSphere, refinedError);
    }

    // Returns the LOD of this cluster
    public uint getLOD() {
        return lod;
    }

    public float4 getBoundingSphere() {
        return boundingSphere;
    }
};

public struct ClusterInstance {
    public uint32_t clusterID;
    public uint32_t materialIndex;
    public uint32_t instanceDataIndex;
};

public struct ClusterBasedModelData {
    public InstanceData instanceData;
    public uint8_t visible;
    uint8_t pad[15]; // TODO: remove
};
