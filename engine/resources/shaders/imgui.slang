import modules.base;

// mostly based on ImGui's Vulkan backend

struct Vertex {
    float2 position;
    float2 uv;
    uint color;
}

struct VSOutput {
    float4 position : SV_POSITION;
    float2 uv : UV0;
    float4 color : COLOR0;
}

struct DisplayRect {
    float2 translation;
    float2 size;
    uint textureIndex;
}

Sampler2D textures[1024];

[shader("vertex")]
VSOutput vertex(uniform DisplayRect display, Vertex v) {
    VSOutput o;
    float alpha = ((v.color >> 24) & 0xFFu) / 255.0f;
    float blue = ((v.color >> 16) & 0xFFu) / 255.0f;
    float green = ((v.color >> 8) & 0xFFu) / 255.0f;
    float red = ((v.color >> 0) & 0xFFu) / 255.0f;
    o.position = float4(v.position * display.size + display.translation, 0, 1);
    o.uv = v.uv;
    o.color = float4(red, green, blue, alpha);
    return o;
}

[shader("pixel")]
float4 pixel(uniform DisplayRect display, VSOutput v) {
    return textures[NonUniformResourceIndex(display.textureIndex)].Sample(v.uv) * v.color;
}

struct InspectParams {
    float4x4 colorTransform;
    float4 colorOffset;

    float2 textureSize;
    float premultiplyAlpha;
    float disableFinalAlpha;

    float3 backgroundColor;
    bool forceNearestSampling;

    float4 grid;
    float2 gridWidth;
}

// adapted from tex_inspect_opengl.cpp
[shader("pixel")]
float4 imgTexInspect(uniform DisplayRect display, uniform InspectParams params, VSOutput v) {
    float2 texel = v.uv * params.textureSize;
    float2 uv;
    if(params.forceNearestSampling) {
        uv = (floor(texel) + 0.5f) / params.textureSize;
    } else {
        uv = v.uv;
    }
    float2 texelEdge = step(fmod(texel, float2(1.0)), params.gridWidth);
    float isGrid = max(texelEdge.x, texelEdge.y);
    float4 ct = params.colorTransform * textures[NonUniformResourceIndex(display.textureIndex)].Sample(uv) + params.colorOffset;
    ct.rgb = ct.rgb * lerp(1.0, ct.a, params.premultiplyAlpha);
    ct.rgb += params.backgroundColor * (1 - ct.a);
    ct.a = lerp(ct.a, 1, params.disableFinalAlpha);
    ct = lerp(ct, float4(params.grid.rgb, 1), params.grid.a * isGrid);
    return ct;
}