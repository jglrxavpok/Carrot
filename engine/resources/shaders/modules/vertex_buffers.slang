module vertex_buffers;

// Default format of vertices for Carrot
public interface IVertex {
    public float4 getPos();
    public float3 getColor();
    public float3 getNormal();
    public float4 getTangent();
    public float2 getUV();
}

public struct Vertex: IVertex {
    public float4 pos;
    public float3 color;
    private float _pad0;
    public float3 normal;
    private float _pad1;
    public float4 tangent;
    public float2 uv;
    private float2 _pad2;

    public float4 getPos() {
        return pos;
    }

    public float3 getColor() {
        return color;
    }

    public float3 getNormal() {
        return normal;
    }

    public float4 getTangent() {
        return tangent;
    }

    public float2 getUV() {
        return uv;
    }
}

public struct PackedVertex: IVertex {
    public float4 pos;
    public float3 color;
    public float3 normal;
    public float4 tangent;
    public float2 uv;

    public float4 getPos() {
        return pos;
    }

    public float3 getColor() {
        return color;
    }

    public float3 getNormal() {
        return normal;
    }

    public float4 getTangent() {
        return tangent;
    }

    public float2 getUV() {
        return uv;
    }
}

public struct VertexWithBones {
    public Vertex vertex;
    public float4 boneWeights;
    public uint8_t4 boneIDs;
}