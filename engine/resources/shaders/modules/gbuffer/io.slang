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
        result.albedo = albedo.Sample(gNearestSampler, uv);
        result.viewPosition = viewPos.Sample(gNearestSampler, uv).xyz;

        const float4 compressedTBN = viewNormalTangents.Sample(gNearestSampler, uv);
        
        const IntProperties properties = IntProperties(intPropertiesInput.Sample(uv).r);
        result.intProperty = properties;
        const bool negativeNormal = properties & IntProperties::NegativeViewNormalZ;
        const bool negativeTangent = properties & IntProperties::NegativeViewTangentZ;
        const bool negativeBitangent = properties & IntProperties::NegativeViewBitangentZ;
        decompressTBN(compressedTBN, negativeNormal, negativeTangent, negativeBitangent, result.viewTBN);

        result.entityID = entityID.Sample(gNearestSampler, uv);

        const float4 metallicRoughnessVelocityXY = metallicRoughnessValuesVelocityXY.Sample(gNearestSampler, uv);

        result.metallicness = metallicRoughnessVelocityXY.x;
        result.roughness = metallicRoughnessVelocityXY.y;

        const float4 emissiveAndVelocityZ = emissiveValuesVelocityZ.Sample(gNearestSampler, uv);
        result.emissiveColor = emissiveAndVelocityZ.xyz;
        result.motionVector = float3(metallicRoughnessVelocityXY.zw, emissiveAndVelocityZ.w); // motion vector, in clip space
        return result;
    }
}