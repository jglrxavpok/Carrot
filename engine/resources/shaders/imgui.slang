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