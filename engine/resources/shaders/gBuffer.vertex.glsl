#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

// Per vertex
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec2 inUV;

// Per instance
layout(location = 5) in vec4 inInstanceColor;
layout(location = 6) in uvec4 inUUID;
layout(location = 7) in mat4 inInstanceTransform;
layout(location = 11) in mat4 inLastFrameInstanceTransform;


layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec3 outViewPos;
layout(location = 3) out vec3 outPreviousFrameViewPos;
layout(location = 4) flat out uvec4 outUUID;
layout(location = 5) out vec3 T;
layout(location = 6) out vec3 N;
layout(location = 7) out float outBitangentSign;
layout(location = 8) out flat mat4 outModelview;
layout(location = 12) out flat int outDrawID;

void main() {
    outDrawID = gl_DrawID;

    uv = inUV;
    mat4 modelview = cbo.view * inInstanceTransform;
    mat4 previousFrameModelview = previousFrameCBO.view * inLastFrameInstanceTransform;
    vec4 viewPosition = modelview * inPosition;
    vec4 previousFrameViewPosition = previousFrameModelview * inPosition;
    gl_Position = cbo.jitteredProjection * viewPosition;

    vColor = vec4(inColor, 1) * inInstanceColor;
    outViewPos = viewPosition.xyz / viewPosition.w;
    outPreviousFrameViewPos = previousFrameViewPosition.xyz / previousFrameViewPosition.w;

    vec3 t = normalize(inTangent.xyz);
    vec3 n = normalize(inNormal);

    mat3 normalMatrix = mat3(modelview);
    normalMatrix = transpose(inverse(normalMatrix));
    N = normalize(normalMatrix * n);
    T = normalize(normalMatrix * (t - dot(t, n) * n));

    outBitangentSign = inTangent.w;

    outUUID = inUUID;
    outModelview = modelview;
}