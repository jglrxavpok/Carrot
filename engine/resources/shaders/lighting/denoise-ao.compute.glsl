#define PIXEL_TYPE float
#define PIXEL_SWIZZLE(v) (v).r
#define PIXEL_CONVERT_TO_VEC4(p) (p).rrrr

#include "denoise-base.compute.glsl"