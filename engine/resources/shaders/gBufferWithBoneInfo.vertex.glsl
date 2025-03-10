const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 20;

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

// Per vertex
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec2 inUV;
layout(location = 5) in ivec4 boneIDs;
layout(location = 6) in vec4 boneWeights;

// Per instance
layout(location = 7) in vec4 inInstanceColor;
layout(location = 8) in uvec4 inInstanceUUID;
layout(location = 9) in mat4 inInstanceTransform;
layout(location = 13) in mat4 inLastFrameInstanceTransform;
layout(location = 17) in uint animationIndex;
layout(location = 18) in float animationTime;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out vec3 outViewNormal;
layout(location = 5) out uvec4 outUUID;
layout(location = 6) out mat3 outTBN;

void main() {
    uv = inUV;
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * inPosition;
    gl_Position = cbo.jitteredProjection * viewPosition;

    fragColor = inColor;
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz;

    outViewNormal = normalize((transpose(inverse(mat3(modelview)))) * inNormal);
    outTBN = mat3(vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
    outUUID = uvec4(0);
}