#include "gbuffer.glsl"

#define DEFINE_GBUFFER_INPUTS(setID)                                                                                    \
layout(set = setID, binding = 0) uniform texture2D albedo;                                                              \
layout(set = setID, binding = 1) uniform texture2D depth;                                                               \
layout(set = setID, binding = 2) uniform texture2D viewPos;                                                             \
layout(set = setID, binding = 3) uniform texture2D viewNormalTangents;                                                  \
layout(set = setID, binding = 4) uniform usampler2D intPropertiesInput;                                                 \
                                                                                                                        \
layout(set = setID, binding = 5) uniform texture2D _unused;                                                             \
                                                                                                                        \
layout(set = setID, binding = 6) uniform texture2D motionVectors;                                                       \
layout(set = setID, binding = 7) uniform texture2D metallicRoughnessValues;                                             \
layout(set = setID, binding = 8) uniform texture2D emissiveValues;                                                      \
layout(set = setID, binding = 9) uniform samplerCube skybox3D;                                                          \
layout(set = setID, binding = 10) uniform texture2D _unused2;                                                           \
layout(set = setID, binding = 11) uniform sampler gLinearSampler;                                                       \
layout(set = setID, binding = 12) uniform sampler gNearestSampler;

GBuffer unpackGBuffer()
{
    GBuffer gbuffer;
    // TODO
    return gbuffer;
}