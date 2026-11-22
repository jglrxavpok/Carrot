// Input/output structures for working with the GBuffer

implementing gbuffer;

import glsl;

// Expected format for the output of vertex shaders that will be passed to a gbuffer type pixel shader
public struct VSGBufferOutput {
    public vec4 vertexColor;
    public vec2 uv;
    public vec3 viewPosition;
    public vec3 previousFrameViewPosition;
    public nointerpolation uvec4 uuid;
    public vec3 T;
    public vec3 N;
    public nointerpolation float bitangentSign;
    public nointerpolation mat4 inModelview;
    public nointerpolation int drawID;

    public vec4 svPos: SV_Position;

    public float clipDistances[1] : SV_ClipDistance;
}

// Intended to be used in a ParameterBlock<>
public struct GBufferInputs {
    public Texture2D albedo;
    public Texture2D viewPos;
    public Texture2D<uint16_t4> viewNormalTangents;
    public Texture2D<uvec4> entityID;
    public Texture2D metallicRoughnessValuesVelocityXY;
    public Texture2D emissiveValues;
    public Texture2D depth;
    public SamplerCube skybox3D;
    public SamplerState linearSampler;
    public SamplerState gNearestSampler;

    public GBuffer read(float2 uv) {
        GBuffer result;
        
        int2 dimensions;
        albedo.GetDimensions(dimensions.x, dimensions.y);

        int2 pixelCoords = int2(uv * dimensions);

        result.albedo = albedo.Load(int3(pixelCoords, 0));

        float4 viewPosData = viewPos.Load(int3(pixelCoords, 0));
        result.viewPosition = viewPosData.xyz;
        result.readIsSky = viewPosData.w < 0.5f;
        result.readDepth = depth.Load(int3(pixelCoords, 0)).r;

        const uint16_t4 compressedTBN = viewNormalTangents.Load(int3(pixelCoords, 0));
        
        decompressTBN(compressedTBN, result.viewTBN);

        result.entityID = entityID.Load(int3(pixelCoords, 0));

        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.Load(int3(pixelCoords, 0));

        result.metallicness = metallicRoughnessVelocityXY.x;
        result.roughness = metallicRoughnessVelocityXY.y;

        result.emissiveColor = emissiveValues.Load(int3(pixelCoords, 0)).rgb;
        result.motionVector = metallicRoughnessVelocityXY.zw; // motion vector, in UV space
        return result;
    }

    public float2 readMotionVector(float2 at) {
        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.SampleGrad(gNearestSampler, at, float2(0), float2(0));
        return metallicRoughnessVelocityXY.zw;
    }
}

public struct RWGBufferInputs {
    public RWTexture2D albedo;
    public RWTexture2D viewPos;
    public RWTexture2D<uint16_t4> viewNormalTangents;
    public RWTexture2D<uvec4> entityID;
    public RWTexture2D metallicRoughnessValuesVelocityXY;
    public RWTexture2D emissiveValues;
    public Texture2D depth; // depth not writtable
    public SamplerCube skybox3D;
    public SamplerState linearSampler;
    public SamplerState gNearestSampler;

    public GBuffer read(float2 uv) {
        GBuffer result;
        int2 dimensions;
        albedo.GetDimensions(dimensions.x, dimensions.y);
        
        const int2 coords = uv * dimensions;
        result.albedo = albedo.Load(coords);

        float4 viewPosData = viewPos.Load(coords);
        result.viewPosition = viewPosData.xyz;
        result.readIsSky = viewPosData.w < 0.5f;
        result.readDepth = depth.Load(float3(coords, 0)).r;

        const uint16_t4 compressedTBN = viewNormalTangents.Load(coords);
        
        decompressTBN(compressedTBN, result.viewTBN);

        result.entityID = entityID.Load(coords);

        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.Load(coords);

        result.metallicness = metallicRoughnessVelocityXY.x;
        result.roughness = metallicRoughnessVelocityXY.y;

        result.emissiveColor = emissiveValues.Load(coords).rgb;
        result.motionVector = metallicRoughnessVelocityXY.zw; // motion vector, in UV space
        return result;
    }

    public float2 readMotionVector(float2 at) {
        int2 dimensions;
        albedo.GetDimensions(dimensions.x, dimensions.y);
        
        const int2 coords = at * dimensions;
        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.Load(coords);
        return metallicRoughnessVelocityXY.zw;
    }
}
