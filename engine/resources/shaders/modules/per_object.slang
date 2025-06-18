module per_object;
import base;

public struct ObjectDrawData {
    public uint materialIndex;

    public uvec4 uuid;
}

public struct PerObjectDrawBuffers {
    public StructuredBuffer<ObjectDrawData, ScalarDataLayout> drawData;
    public StructuredBuffer<uint> offsets;

    public ObjectDrawData Load(uint drawIndex) {
        return drawData[offsets.Load(0) + drawIndex];
    }
}