import modules.base;
import modules.gbuffer;
import modules.materials;
import modules.virtual_geometry;
import modules.vertex_buffers;
import modules.camera;
import modules.viewport;

// TODO: merge with draw_virtual_geometry
struct IO {
    RWTexture2D<uint64_t> visibilityBuffer;
    StructuredBuffer<Cluster, ScalarDataLayout> clusters;
    StructuredBuffer<ClusterInstance, ScalarDataLayout> clusterInstances;
    StructuredBuffer<ClusterBasedModelData, ScalarDataLayout> modelData;
}

ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<IO> io;
ParameterBlock<Camera> cameras;
ParameterBlock<Viewport> viewport;

struct PixelIn {
    float2 screenUV;
    float4 svPos: SV_Position;
}

// https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf
// Deferred Attribute Interpolation for Memory-Efficient Deferred Shading
// Scheid, Dachsbacher
struct Interpolator {
    private float3 invWi;
    private float3 barycentrics;
    private float invDivider; // 1 / (wi * barycentrics)

    __init(float2 screenUV, const PackedVertex a, const PackedVertex b, const PackedVertex c, const float4x4 modelview, float ndcDepth) {
        const float2 ndcUV = screenUV * 2 - 1;
        float4 pointInViewSpace = cameras.CurrentFrame().inverseNonJitteredProjection * float4(float3(ndcUV, ndcDepth), 1);
        pointInViewSpace.xyz *= pointInViewSpace.w;

        const float4x4 clipSpaceFromMesh = cameras.CurrentFrame().jitteredProjection * modelview;
        float4 aClipSpace = clipSpaceFromMesh * a.getPos();
        float4 bClipSpace = clipSpaceFromMesh * b.getPos();
        float4 cClipSpace = clipSpaceFromMesh * c.getPos();

        this.invWi = 1.0f / float3(aClipSpace.w, bClipSpace.w, cClipSpace.w);
        aClipSpace.xyz /= aClipSpace.w;
        bClipSpace.xyz /= bClipSpace.w;
        cClipSpace.xyz /= cClipSpace.w;

        // barycentrics computation
        {
            const float D = determinant(float2x2(cClipSpace.xy - bClipSpace.xy, aClipSpace.xy - bClipSpace.xy));
            float lambda1 = (bClipSpace.y - cClipSpace.y) * (ndcUV.x - cClipSpace.x) + (cClipSpace.x - bClipSpace.x) * (ndcUV.y - cClipSpace.y);
            float lambda2 = (cClipSpace.y - aClipSpace.y) * (ndcUV.x - cClipSpace.x) + (aClipSpace.x - cClipSpace.x) * (ndcUV.y - cClipSpace.y);
            lambda1 /= D;
            lambda2 /= D;
            this.barycentrics = float3(lambda1, lambda2, 1 - lambda1 - lambda2);
        }
        this.invDivider = 1.0f / dot(this.invWi, this.barycentrics);
    }

    vector<float, N> interpolate<int N>(vector<float, N> a, vector<float, N> b, vector<float, N> c) {
        vector<float, N> numerator = a * barycentrics.x * invWi.x
            + b * barycentrics.y * invWi.y
            + c * barycentrics.z * invWi.z;

        return numerator * invDivider;
    }
}

[shader("pixel")]
EncodedGBuffer main(in PixelIn input, out float outDepth : SV_Depth) {
    uint2 size;
    io.visibilityBuffer.GetDimensions(size.x, size.y);

    const int2 pixelCoords = int2(size * input.screenUV + 0.5);
    const uint64_t visibilityBufferSample = io.visibilityBuffer.Load(pixelCoords);
    if(visibilityBufferSample == 0) {
        discard;
    }    

    const double depth = uint(0xFFFF_FFFFu - (visibilityBufferSample >> 32u)) / double(0xFFFF_FFFFu);
    outDepth = depth;

    const uint32_t low = uint32_t(visibilityBufferSample);
    const uint32_t triangleIndex = low & 0x7Fu;
    const uint32_t instanceIndex = (low >> 7) & 0x1FF_FFFFu;
    const ClusterInstance clusterInstance = io.clusterInstances[instanceIndex];
    const Cluster cluster = io.clusters[clusterInstance.clusterID];
    const Material material = materialSystem.materials[clusterInstance.materialIndex];
    
    const PackedVertex vA = cluster.vertices[cluster.indices[triangleIndex * 3 + 0]];
    const PackedVertex vB = cluster.vertices[cluster.indices[triangleIndex * 3 + 1]];
    const PackedVertex vC = cluster.vertices[cluster.indices[triangleIndex * 3 + 2]];

    const float4x4 clusterTransform = transpose(float4x4(transpose(cluster.transform), float4(0,0,0,1)));
    const ClusterBasedModelData modelData = io.modelData[clusterInstance.instanceDataIndex];
    const float4x4 modelview = cameras.CurrentFrame().view * modelData.instanceData.transform * clusterTransform;
    
    const Interpolator interpolator = Interpolator(input.screenUV, vA, vB, vC, modelview, float(depth));

    const float2 surfaceUV = interpolator.interpolate(vA.uv, vB.uv, vC.uv);
    const MaterialOutput materialOutput = material.Compute(materialSystem, gBufferInputs.linearSampler, surfaceUV);

    if(materialOutput.albedo.a < 0.01) {
        discard;
    }

    const float4 localSpacePos = interpolator.interpolate(vA.getPos(), vB.getPos(), vC.getPos());
    const float4 viewPos = modelview * localSpacePos;

    GBuffer gbuffer;
    gbuffer.viewPosition = viewPos.xyz;

    gbuffer.intProperty = IntProperties::RayTracedLighting;
    gbuffer.entityID = modelData.instanceData.uuid;

    gbuffer.albedo = materialOutput.albedo * modelData.instanceData.color;
    gbuffer.emissiveColor = materialOutput.emissive;
    gbuffer.metallicness = materialOutput.metallic;
    gbuffer.roughness = materialOutput.roughness;

    // Normal mapping
    {
        float3 N = interpolator.interpolate(vA.normal, vB.normal, vC.normal);
        float3 T = interpolator.interpolate(vA.tangent.xyz, vB.tangent.xyz, vC.tangent.xyz);

        float3x3 normalMatrix = float3x3(modelview);
        normalMatrix = transpose(inverse(normalMatrix));
        N = normalMatrix * N;
        T = normalMatrix * T;
        float bitangentSign = interpolator.interpolate(vA.tangent.w, vB.tangent.w, vC.tangent.w);

        float3 N_ = normalize(N);
        float3 T_ = normalize(T - dot(T, N_) * N_);

        float3 B_ = normalize(bitangentSign * cross(N_, T_));

        float3 mappedNormal = materialOutput.normal;
        mappedNormal = normalize(mappedNormal.x * T_ + mappedNormal.y * B_ + mappedNormal.z * N_);

        N_ = mappedNormal;
        T_ = normalize(T - dot(T, N_) * N_);
        B_ = normalize(bitangentSign * cross(N_, T_));

        gbuffer.viewTBN = mat3(T_, B_, N_);
    }
    
    const float4x4 previousFrameModelView = cameras.PreviousFrame().view * modelData.instanceData.transform * clusterTransform;
    float4 previousClipPos = cameras.PreviousFrame().nonJitteredProjection * previousFrameModelView * localSpacePos;
    previousClipPos.xy /= previousClipPos.w;

    previousClipPos.xy = previousClipPos.xy * float2(0.5f, 0.5f) + 0.5f;
    previousClipPos.xy *= float2(viewport.frameDimensions);
    gbuffer.motionVector.xy = previousClipPos.xy - input.svPos.xy;
    gbuffer.motionVector.xy /= float2(viewport.frameDimensions);
    gbuffer.motionVector.xy += (cameras.CurrentFrame().jitter + cameras.PreviousFrame().jitter) * 0.5f;


    return EncodedGBuffer(gbuffer);
}