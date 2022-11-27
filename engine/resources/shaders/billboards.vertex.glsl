#extension GL_ARB_separate_shader_objects : enable

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

#include "includes/billboards.glsl"

// Per vertex
layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outViewPos;

void main() {
    outUV = (inPosition + 1.0) / 2.0;


    mat4 modelview = cbo.view;
    vec3 worldPos = (mat3(cbo.inverseView) * vec3(inPosition.x, inPosition.y, 0)/2) * billboard.scale + billboard.position;
    vec4 viewPosition = modelview * vec4(worldPos, 1.0);
    gl_Position = cbo.projection * viewPosition;

    outViewPos = viewPosition.xyz;
}