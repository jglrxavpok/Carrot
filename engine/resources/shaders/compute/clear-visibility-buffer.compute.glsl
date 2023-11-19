#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_image_int64 : enable

layout (local_size_x = 32) in;
layout (local_size_y = 32) in;

layout(r64ui, set = 0, binding = 0) uniform writeonly u64image2D outputImage;

void main() {
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const ivec2 size = imageSize(outputImage);

    if(coords.x >= 0
    && coords.y >= 0
    && coords.x < size.x
    && coords.y < size.y) {
        imageStore(outputImage, coords, u64vec4(0ul));
    }
}