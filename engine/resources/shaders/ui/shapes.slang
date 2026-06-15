import ui_common;
import modules.materials;

ParameterBlock<MaterialSystem> materialSystem;

[shader("pixel")]
float4 rectangle(in VSOutput input) {
    return input.color;
}

[shader("pixel")]
float4 image(uniform DisplayRect display, in VSOutput input) {
    return materialSystem.Sample(display.textureIndex, materialSystem.linearSampler, input.uv);
    //return float4(0, 0, 0, 0);
}