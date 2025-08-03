#include <includes/camera.glsl>
DEFINE_CAMERA_SET(1)

// Per vertex
layout(location = 0) in vec3 inPosition;

// Per instance
layout(location = 1) in vec4 inInstanceColor;
layout(location = 2) in uvec4 inUUID;
layout(location = 3) in mat4 inInstanceTransform;


layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out flat mat4 outModelview;
layout(location = 8) out flat int outDrawID;

void main() {
    outDrawID = gl_DrawID;
    uv = inPosition.xy;
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * vec4(inPosition, 1.0);
    gl_Position = cbo.jitteredProjection * viewPosition;

    fragColor = vec4(1.0);
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz;
    outModelview = modelview;
}