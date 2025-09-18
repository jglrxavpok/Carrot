import modules.base;
import modules.camera;
import modules.materials;
import modules.gbuffer;

struct Billboard {
    float3 position;
    float scale;
    uint4 uuid;
    float3 color;
    uint textureID;
}

struct VSOutput {
    float4 pos: SV_Position;
    float2 uv;
    float3 viewPos;
    nointerpolation uint4 uuid;
}

ParameterBlock<Camera> cameras;
ParameterBlock<MaterialSystem> materials;

[shader("vertex")]
VSOutput vertex(uniform Billboard billboard, float2 inPosition) {
    VSOutput o;
    float3 worldPos = float3x3(cameras.CurrentFrame().inverseView) * float3(inPosition, 0)/2 * billboard.scale + float3(billboard.position);
    float4 viewPos = cameras.CurrentFrame().view * float4(worldPos, 1);
    o.pos = cameras.CurrentFrame().jitteredProjection * viewPos;
    o.viewPos = viewPos.xyz;
    o.uv = (inPosition + 1) / 2;
    o.uuid = billboard.uuid;
    return o;
}

[shader("pixel")]
EncodedGBuffer pixel(uniform Billboard billboard, VSOutput v) {
    GBuffer gbuffer;

    float4 texColor = materials.Sample(billboard.textureID, materials.linearSampler, v.uv) * float4(billboard.color, 1);
    if(texColor.a < 0.01) {
        discard;
    }
    gbuffer.albedo = float4(texColor.rgb, 1);
    gbuffer.viewPosition = v.viewPos;
    gbuffer.entityID = v.uuid;

    return EncodedGBuffer(gbuffer);
}