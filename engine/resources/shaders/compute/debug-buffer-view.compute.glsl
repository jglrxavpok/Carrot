#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

layout (local_size_x = 32) in;
layout (local_size_y = 32) in;

layout(set = 0, binding = 0) uniform writeonly image2D outputImage;

layout(buffer_reference, scalar) buffer Uint8s {
    uint8_t v[];
};

layout(push_constant, scalar) uniform PushConstant {
    Uint8s uints;
    uint bufferSize;
} push;

float fetchValue(uint index) {
    if(index >= push.bufferSize) {
        const ivec2 size = imageSize(outputImage);
        uint blockSize = 40;
        uint blockX = (index / size.x) / blockSize;
        uint blockY = (index % size.x) / blockSize;

        vec3 carrotColor = vec3(0xFF, 0x81, 0x46);
        return ((blockX ^ blockY) & 1u) * carrotColor[index % 3] / 255.0f;
    }

    return push.uints.v[index] / 255.0f;
}

void main() {
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const ivec2 size = imageSize(outputImage);
    if(coords.x >= size.x || coords.y >= size.y) return;

    uint index = (coords.x + coords.y * size.x)*3;
    vec3 pixel = vec3(fetchValue(index), fetchValue(index+1), fetchValue(index+2));

    imageStore(outputImage, coords, vec4(pixel, 1));
}