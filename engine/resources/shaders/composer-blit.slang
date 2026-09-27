import glsl;

SamplerState linearSampler;
Texture2D textures[];
Texture2D depthTextures[];

struct ScreenQuadVertex {
    float2 uv;
    nointerpolation uint textureIndex;
    nointerpolation uint discardWhereDepthIsUnwritten;
}

[shader("pixel")]
float4 main(in ScreenQuadVertex input) {
    if (input.discardWhereDepthIsUnwritten != 0) {
        float depth = depthTextures[input.textureIndex].Sample(linearSampler, input.uv).r;
        if (depth == 1.0f) {
            discard;
        }
    }
    return textures[input.textureIndex].Sample(linearSampler, input.uv);
}