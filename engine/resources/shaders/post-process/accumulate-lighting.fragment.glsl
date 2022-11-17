layout(set = 0, binding = 0) uniform texture2D currentFrame;
layout(set = 0, binding = 1) uniform texture2D previousFrame;
layout(set = 0, binding = 2) uniform sampler nearestSampler;
layout(set = 0, binding = 3) uniform sampler linearSampler;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main() {
    // TODO: reprojection
    vec4 currentFrameColor = texture(sampler2D(currentFrame, linearSampler), uv);
    vec4 previousFrameColor = texture(sampler2D(previousFrame, linearSampler), uv);

    float alpha = 0.25;
    if(isnan(previousFrameColor.r) || isnan(previousFrameColor.g) || isnan(previousFrameColor.b))
    {
        outColor = currentFrameColor;
    }
    else
    {
        outColor = vec4((previousFrameColor * alpha + currentFrameColor * (1-alpha)).rgb, 1.0);
    }
}