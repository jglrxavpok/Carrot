// Per vertex
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 uv;

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

void main() {
    mat3 rot = mat3(
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, 1.0, 0.0)
    );
    uv = rot * inPosition;
    mat4 viewNoTranslation = mat4(mat3(cbo.view));
    gl_Position = cbo.jitteredProjection * viewNoTranslation * vec4(inPosition, 1);
}