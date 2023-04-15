#define DEFINE_VIEWPORT_CONSTANT(setID)                                                                                 \
layout(set = setID, binding = 0) uniform ViewportConstant {                                                             \
    uint frameCount;                                                                                                    \
    uint frameWidth;                                                                                                    \
    uint frameHeight;                                                                                                   \
                                                                                                                        \
    bool hasTLAS;                                                                                                       \
} viewport;