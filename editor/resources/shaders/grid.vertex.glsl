#include "../../../engine/resources/shaders/includes/camera.glsl"
DEFINE_CAMERA_SET(0)

// Per vertex
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec2 outVertexPosition;
layout(location = 1) out vec3 outViewPosition;
layout(location = 2) out flat mat4 outModelview;

#include "grid.glsl"

void main() {
    outVertexPosition = inPosition.xy * grid.size;
    mat4 modelview = cbo.view;
    vec4 viewPosition = modelview * vec4(inPosition * grid.size, 1.0);
    outViewPosition = viewPosition.xyz / viewPosition.w;
    gl_Position = cbo.projection * viewPosition;
    outModelview = modelview;
}