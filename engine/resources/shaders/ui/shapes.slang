import ui_common;
import modules.materials;

ParameterBlock<MaterialSystem> materialSystem;

bool checkAgainstScissor(in DisplayRect display, float2 pixelPos) {
    if (display.drawFlags & DrawFlags::HasScissor) {
        return true;//all(pixelPos >= display.scissorOffset && pixelPos < display.scissorOffset + display.scissorOffset);
    }
    return true;
}

float4 applyOverlay(in DisplayRect display, float4 color) {
    return float4(lerp(color.rgb, display.overlayColor.rgb, display.overlayColor.a), color.a);
}

[shader("pixel")]
float4 rectangle(uniform DisplayRect display, in VSOutput input) {
    if (!checkAgainstScissor(display, input.position.xy)) {
        discard;
    }
    return applyOverlay(display, input.color);
}

[shader("pixel")]
float4 image(uniform DisplayRect display, in VSOutput input) {
    if (!checkAgainstScissor(display, input.position.xy)) {
        discard;
    }
    float4 baseColor = float4(1, 1, 1, 1);
    if (display.textureIndex != 0xFFFFFFFF) {
        baseColor = materialSystem.Sample(display.textureIndex, materialSystem.linearSampler, input.uv);
    }
    float4 finalColor = baseColor * input.color;
    return applyOverlay(display, finalColor);
}