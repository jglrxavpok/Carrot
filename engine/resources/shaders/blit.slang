[shader("pixel")]
float4 blit(
    uniform Texture2D<float4> input,
    uniform SamplerState sampler,
    uniform bool gammaCorrect,
    in float2 uv) {

    if (gammaCorrect) {
        return pow(input.Sample(sampler, uv), 1.0/2.2);
    } else {
        return input.Sample(sampler, uv);
    }
}