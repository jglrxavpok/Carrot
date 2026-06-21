import modules.base;
import modules.viewport;
import modules.camera;


public struct Vertex {
    public float2 position;
    public float2 uv;
    public uint color;
}

public struct VSOutput {
    public float4 position : SV_POSITION;
    public float2 uv : UV0;
    public float4 color : COLOR0;
}

public struct DisplayRect {
    public float4x4 transform;
    public uint textureIndex;
    public uint32_t inWorld;
}

[vk::binding(0, 0)]
ParameterBlock<Viewport> viewport;

[vk::binding(0, 1)]
ParameterBlock<Camera> camera;

[shader("vertex")]
VSOutput vertex(Vertex v, uniform DisplayRect pushData) {
    VSOutput o;
    float alpha = ((v.color >> 24) & 0xFFu) / 255.0f;
    float blue = ((v.color >> 16) & 0xFFu) / 255.0f;
    float green = ((v.color >> 8) & 0xFFu) / 255.0f;
    float red = ((v.color >> 0) & 0xFFu) / 255.0f;
    const float4x4 projview = camera.CurrentFrame().jitteredProjection * camera.CurrentFrame().view;
    float4 vertexPos = float4((v.position / viewport.frameDimensions) * 2.0 - 1, 0, 1);
    if (pushData.inWorld) {
        vertexPos = projview * pushData.transform * vertexPos;
    }
    o.position = vertexPos;
    o.uv = v.uv;
    o.color = float4(red, green, blue, alpha);
    return o;
}
