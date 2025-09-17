SamplerState linearSampler;
Texture2D textures[];

struct ScreenQuadVertex {
    float2 uv;
    nointerpolation uint textureIndex;
}

[shader("pixel")]
float4 main(in ScreenQuadVertex input) {
    return textures[input.textureIndex].Sample(linearSampler, input.uv);
}