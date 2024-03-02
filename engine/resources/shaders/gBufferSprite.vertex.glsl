#extension GL_EXT_nonuniform_qualifier : enable

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

// Per vertex
layout(location = 0) in vec3 inPosition;

// Per instance
layout(location = 1) in vec4 inInstanceColor;
layout(location = 2) in uvec4 inUUID;
layout(location = 3) in mat4 inInstanceTransform;
layout(location = 7) in mat4 inLastFrameInstanceTransform;


layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out uvec4 outUUID;
layout(location = 5) out flat mat4 outModelview;
layout(location = 9) out flat int outDrawID;

layout(push_constant) uniform TexRegion {
    vec2 center;
    vec2 halfSize;
} region;

void main() {
    outDrawID = gl_DrawID;
    uv = (inPosition.xy+vec2(0.5)) * (region.halfSize * 2) + region.center - region.halfSize;
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * vec4(inPosition, 1.0);
    gl_Position = cbo.jitteredProjection * viewPosition;

    fragColor = vec4(1.0);
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz;

    outUUID = inUUID;
    outModelview = modelview;
}