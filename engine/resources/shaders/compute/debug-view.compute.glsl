layout (local_size_x = 32) in;
layout (local_size_y = 32) in;

layout(set = 0, binding = 0) uniform texture2D inputImage;
layout(set = 0, binding = 1) uniform sampler linearSampler;
layout(set = 0, binding = 2) uniform sampler nearestSampler;
layout(set = 0, binding = 3) uniform writeonly image2D outputImage;

void main() {
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const ivec2 size = imageSize(outputImage);

    vec2 uv = vec2(coords) / size;
    vec4 pixel = texture(sampler2D(inputImage, linearSampler), uv);
    imageStore(outputImage, coords, vec4(pixel.rgb, 1));
}