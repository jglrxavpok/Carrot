layout(set = 0, binding = 0) uniform texture2D albedo;
layout(set = 0, binding = 1) uniform texture2D skybox;
layout(set = 0, binding = 2) uniform texture2D depth;
layout(set = 0, binding = 3) uniform texture2D lighting;

layout(set = 0, binding = 4) uniform sampler nearestSampler;
layout(set = 0, binding = 5) uniform sampler linearSampler;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 albedoColor = texture(sampler2D(albedo, linearSampler), uv);
    vec4 lightingColor = texture(sampler2D(lighting, linearSampler), uv);

    float currDepth = texture(sampler2D(depth, linearSampler), uv).r;

    if(currDepth < 1.0) {
        outColor = vec4(albedoColor.rgb * lightingColor.rgb, 1.0);
    } else {
        outColor = vec4(lightingColor.rgb, 1.0);
    }
}