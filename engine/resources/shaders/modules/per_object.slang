module per_object;
import base;

public struct ObjectDrawData {
    public uint materialIndex;

    public uvec4 uuid;
}

public struct PerObjectDrawBuffers {
    public StructuredBuffer<ObjectDrawData, ScalarDataLayout> drawData;
    public ConstantBuffer<uint> offset;

    public ObjectDrawData Load(uint drawIndex) {
        return drawData[offset + drawIndex];
    }
}