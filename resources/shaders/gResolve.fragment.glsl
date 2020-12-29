#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, binding = 0) uniform subpassInput albedo;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput depth;
layout(input_attachment_index = 2, binding = 2) uniform subpassInput viewPos;
layout(input_attachment_index = 3, binding = 3) uniform subpassInput viewNormals;
layout(input_attachment_index = 4, binding = 4) uniform subpassInput raytracedLighting;

layout(set = 0, binding = 5) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
} cbo;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

const vec3 sunDirection = vec3(1,1,1);

void main() {
    const vec3 nSunDirection = normalize(sunDirection);
    vec4 fragmentColor = subpassLoad(albedo);
    vec3 normal = (cbo.inverseView * subpassLoad(viewNormals)).xyz;
    float lighting = max(0.1, dot(nSunDirection, normal));
    outColor = vec4(fragmentColor.rgb*lighting, fragmentColor.a);
}