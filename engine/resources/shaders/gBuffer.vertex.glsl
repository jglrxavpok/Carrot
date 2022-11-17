layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

// Per vertex
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec2 inUV;

// Per instance
layout(location = 5) in vec4 inInstanceColor;
layout(location = 6) in uvec4 inUUID;
layout(location = 7) in mat4 inInstanceTransform;


layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out vec3 outViewNormal;
layout(location = 5) out uvec4 outUUID;
layout(location = 6) out mat3 TBN;

void main() {
    uv = inUV;
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * inPosition;
    gl_Position = cbo.projection * viewPosition;

    fragColor = inColor;
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz / viewPosition.w;

#define rotate(vec) normalize(((mat3(modelview))) * vec)

    outViewNormal = rotate(inNormal);

    vec3 T = rotate(inTangent);
    vec3 N = rotate(inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(T, N);
    TBN = mat3(T, B, N);

    outUUID = inUUID;
}