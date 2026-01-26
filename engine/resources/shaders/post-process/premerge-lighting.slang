// Mix reflections with direct lighting and GI in a single texture (which will then get denoised)

import modules.gbuffer;

struct IO {
    RWTexture2D<float4> directLighting;
    RWTexture2D<float4> reflections;
    RWTexture2D<float4> gi;
    RWTexture2D<float4> output;
};
ParameterBlock<IO> io;
ParameterBlock<GBufferInputs> gBufferInputs;

[numthreads(32,32,1)]
[shader("compute")]
void main(uint2 coords: SV_DispatchThreadID) {
    int w, h;
    io.output.GetDimensions(w, h);

    if(coords.x >= w || coords.y >= h) {
        return;
    }

    float2 size = float2(w, h);
    float2 uv = float2(coords) / size;

    const GBuffer gBuffer = gBufferInputs.read(uv);

    float3 diffuseLighting = io.directLighting.Load(coords).rgb;
    const float3 gi = io.gi.Load(coords).rgb;
    const float3 reflectionsValue = io.reflections.Load(coords).rgb;
    diffuseLighting += gi;

    float3 finalColor = lerp(diffuseLighting, reflectionsValue, gBuffer.metallicness);
    io.output.Store(coords, float4(finalColor, 1));
}