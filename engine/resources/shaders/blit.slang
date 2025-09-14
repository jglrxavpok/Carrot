[shader("pixel")]
float4 blit(
    uniform Texture2D<float4> input,
    uniform SamplerState sampler,
    in float2 uv) {

    return input.Sample(sampler, uv);
}