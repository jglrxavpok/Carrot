layout (local_size_x = 1, local_size_y = 1) in;

layout(rgba32f, set = 0, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 0, binding = 1) uniform writeonly image2D outputImage;

void main() {
    ivec2 coords = ivec2(gl_GlobalInvocationID);

    vec4 pixel = imageLoad(inputImage, coords);

    // TODO

    imageStore(outputImage, coords, pixel);
}