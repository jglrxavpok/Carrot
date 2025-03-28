#include <includes/camera.glsl>
DEFINE_CAMERA_SET(0)

// Per vertex
layout(location = 0) in vec4 inPosition;

// Per instance
layout(location = 1) in vec4 inInstanceColor;
layout(location = 2) in uvec4 inUUID;
layout(location = 3) in mat4 inInstanceTransform;


layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out vec3 outViewNormal;
layout(location = 5) out uvec4 outUUID;
layout(location = 6) out mat3 TBN;

void main() {
    uv = vec2(0.5);
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * inPosition;
    gl_Position = cbo.jitteredProjection * viewPosition;

    fragColor = inInstanceColor.rgb;
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz;

    outViewNormal = normalize((transpose(inverse(mat3(modelview)))) * vec3(0.0, 1.0, 0.0));

    TBN = mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

    outUUID = inUUID;
}