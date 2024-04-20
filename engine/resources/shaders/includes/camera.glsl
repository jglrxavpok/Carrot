#define DEFINE_CAMERA_SET(setID)                                                                                        \
layout(set = setID, binding = 0) uniform CameraBufferObject {                                                           \
    mat4 view;                                                                                                          \
    mat4 inverseView;                                                                                                   \
    mat4 jitteredProjection;                                                                                            \
    mat4 nonJitteredProjection;                                                                                         \
    mat4 inverseJitteredProjection;                                                                                     \
    mat4 inverseNonJitteredProjection;                                                                                  \
    vec4 frustumPlanes[6]; /* xyz = normal, w = distance from origin*/                                                  \
} cbo;                                                                                                                  \
                                                                                                                        \
layout(set = setID, binding = 1) uniform LastFrameCameraBufferObject {                                                  \
    mat4 view;                                                                                                          \
    mat4 inverseView;                                                                                                   \
    mat4 jitteredProjection;                                                                                            \
    mat4 nonJitteredProjection;                                                                                         \
    mat4 inverseJitteredProjection;                                                                                     \
    mat4 inverseNonJitteredProjection;                                                                                  \
    vec4 frustumPlanes[6]; /* xyz = normal, w = distance from origin*/                                                  \
} previousFrameCBO;
