#define TEXTURE_FORMAT rgba32f
#define PIXEL_TYPE vec4
#define PIXEL_SWIZZLE(v) (v)
#define PIXEL_CONVERT_TO_VEC4(p) (p)

#include "denoise-base.compute.glsl"