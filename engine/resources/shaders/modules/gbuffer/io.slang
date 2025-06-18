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
    public ConstantBuffer<Texture2D> albedo;
    public ConstantBuffer<Texture2D> viewPos;
    public ConstantBuffer<Texture2D> viewNormalTangents;
    public ConstantBuffer<Sampler2D<uvec4>> intPropertiesInput;
    public ConstantBuffer<Texture2D<IntProperties::__Tag>> entityID;
    public ConstantBuffer<Texture2D> metallicRoughnessValuesVelocityXY;
    public ConstantBuffer<Texture2D> emissiveValuesVelocityZ;
    public ConstantBuffer<Texture2D> _unused;
    public ConstantBuffer<Texture2D> depth;
    public ConstantBuffer<SamplerCube> skybox3D;
    public ConstantBuffer<SamplerState> linearSampler;
    public ConstantBuffer<SamplerState> gNearestSampler;
}