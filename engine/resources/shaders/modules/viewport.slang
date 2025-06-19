module viewport;
import base;

public struct Viewport {
    public uint frameCount;
    public uint frameWidth;
    public uint frameHeight;

    bool hasTLAS;

    public ivec2 ScreenSpaceToPixels(float2 uv) {
        return ivec2(uv * vec2(frameWidth, frameHeight));
    }
}