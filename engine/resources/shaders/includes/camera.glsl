#define DEFINE_CAMERA_SET(setID)                                                                                        \
layout(set = setID, binding = 0) uniform CameraBufferObject {                                                           \
    mat4 projection;                                                                                                    \
    mat4 view;                                                                                                          \
    mat4 inverseView;                                                                                                   \
    mat4 inverseProjection;                                                                                             \
} cbo;                                                                                                                  \
                                                                                                                        \
layout(set = setID, binding = 1) uniform LastFrameCameraBufferObject {                                                  \
    mat4 projection;                                                                                                    \
    mat4 view;                                                                                                          \
    mat4 inverseView;                                                                                                   \
    mat4 inverseProjection;                                                                                             \
} previousFrameCBO;