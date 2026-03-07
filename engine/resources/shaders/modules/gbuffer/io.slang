// Input/output structures for working with the GBuffer

implementing gbuffer;

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
    public nointerpolation int inDrawID;

    public vec4 svPos: SV_Position;
}

// Intended to be used in a ParameterBlock<>
public struct GBufferInputs {
    public Texture2D albedo;
    public Texture2D viewPos;
    public Texture2D viewNormalTangents;
    public Sampler2D<IntProperties::__Tag> intPropertiesInput;
    public Texture2D<uvec4> entityID;
    public Texture2D metallicRoughnessValuesVelocityXY;
    public Texture2D emissiveValuesVelocityZ;
    public Texture2D _unused;
    public Texture2D depth;
    public SamplerCube skybox3D;
    public SamplerState linearSampler;
    public SamplerState gNearestSampler;

    public GBuffer read(float2 uv) {
        GBuffer result;
        result.albedo = albedo.SampleGrad(gNearestSampler, uv, float2(0), float2(0));

        float4 viewPosData = viewPos.SampleGrad(gNearestSampler, uv, float2(0), float2(0));
        result.viewPosition = viewPosData.xyz;
        result.readIsSky = viewPosData.w < 0.5f;
        result.readDepth = depth.SampleGrad(gNearestSampler, uv, float2(0), float2(0)).r;

        const float4 compressedTBN = viewNormalTangents.SampleGrad(gNearestSampler, uv, float2(0), float2(0));
        
        const IntProperties properties = IntProperties(intPropertiesInput.SampleGrad(uv, float2(0), float2(0)).r);
        result.intProperty = properties;
        const bool negativeNormal = properties & IntProperties::NegativeViewNormalZ;
        const bool negativeTangent = properties & IntProperties::NegativeViewTangentZ;
        const bool negativeBitangent = properties & IntProperties::NegativeViewBitangentZ;
        decompressTBN(compressedTBN, negativeNormal, negativeTangent, negativeBitangent, result.viewTBN);

        result.entityID = entityID.SampleGrad(gNearestSampler, uv, float2(0), float2(0));

        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.SampleGrad(gNearestSampler, uv, float2(0), float2(0));

        result.metallicness = metallicRoughnessVelocityXY.x;
        result.roughness = metallicRoughnessVelocityXY.y;

        const float4 emissiveAndVelocityZ = emissiveValuesVelocityZ.SampleGrad(gNearestSampler, uv, float2(0), float2(0));
        result.emissiveColor = emissiveAndVelocityZ.xyz;
        result.motionVector = float3(metallicRoughnessVelocityXY.zw, emissiveAndVelocityZ.w); // motion vector, in UV space
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
    public RWTexture2D viewNormalTangents;
    public RWTexture2D<IntProperties::__Tag> intPropertiesInput;
    public RWTexture2D<uvec4> entityID;
    public RWTexture2D metallicRoughnessValuesVelocityXY;
    public RWTexture2D emissiveValuesVelocityZ;
    public RWTexture2D _unused;
    public RWTexture2D depth;
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
        result.readDepth = depth.Load(coords).r;

        const float4 compressedTBN = viewNormalTangents.Load(coords);
        
        const IntProperties properties = IntProperties(intPropertiesInput.Load(coords).r);
        result.intProperty = properties;
        const bool negativeNormal = properties & IntProperties::NegativeViewNormalZ;
        const bool negativeTangent = properties & IntProperties::NegativeViewTangentZ;
        const bool negativeBitangent = properties & IntProperties::NegativeViewBitangentZ;
        decompressTBN(compressedTBN, negativeNormal, negativeTangent, negativeBitangent, result.viewTBN);

        result.entityID = entityID.Load(coords);

        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.Load(coords);

        result.metallicness = metallicRoughnessVelocityXY.x;
        result.roughness = metallicRoughnessVelocityXY.y;

        const float4 emissiveAndVelocityZ = emissiveValuesVelocityZ.Load(coords);
        result.emissiveColor = emissiveAndVelocityZ.xyz;
        result.motionVector = float3(metallicRoughnessVelocityXY.zw, emissiveAndVelocityZ.w); // motion vector, in UV space
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
