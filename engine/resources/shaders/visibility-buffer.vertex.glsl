#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

// Per vertex
layout(location = 0) in vec4 inPosition;

// Per instance
layout(location = 7) in mat4 inInstanceTransform;

layout(location = 0) out vec4 ndcPosition;
layout(location = 1) out flat int drawID;

void main() {
    drawID = gl_DrawID;

    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * inPosition;

    ndcPosition = cbo.jitteredProjection * viewPosition;
    gl_Position = ndcPosition;
}