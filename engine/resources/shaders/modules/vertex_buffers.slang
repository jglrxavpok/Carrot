module vertex_buffers;

// Default format of vertices for Carrot
public struct Vertex {
    public float4 pos;
    public float3 color;
    public float3 normal;
    public float4 tangent;
    public float2 uv;
}

public struct VertexWithBones {
    public Vertex vertex;
    public float4 boneWeights;
    public uint8_t4 boneIDs;
}