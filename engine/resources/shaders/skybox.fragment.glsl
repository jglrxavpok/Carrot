layout(set = 1, binding = 0) uniform samplerCube image;

layout(location = 0) in vec3 uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 fragmentColor = texture(image, uv);
    outColor = vec4(fragmentColor.rgb, 1.0);
}