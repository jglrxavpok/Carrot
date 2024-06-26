#define DEFINE_GBUFFER_INPUTS(setID)                                                                                    \
layout(set = setID, binding = 0) uniform texture2D gAlbedo;                                                             \
layout(set = setID, binding = 1) uniform texture2D gViewPos;                                                            \
layout(set = setID, binding = 2) uniform texture2D gViewNormalTangents;                                                 \
layout(set = setID, binding = 3) uniform usampler2D gIntPropertiesInput;                                                \
layout(set = setID, binding = 4) uniform utexture2D gEntityID;                                                          \
layout(set = setID, binding = 5) uniform texture2D gMetallicRoughnessValuesVelocityXY;                                  \
layout(set = setID, binding = 6) uniform texture2D gEmissiveValuesVelocityZ;                                            \
layout(set = setID, binding = 7) uniform texture2D _unused;                                                             \
layout(set = setID, binding = 8) uniform texture2D gDepth;                                                              \
layout(set = setID, binding = 9) uniform samplerCube gSkybox3D;                                                         \
layout(set = setID, binding = 10) uniform sampler gLinearSampler;                                                       \
layout(set = setID, binding = 11) uniform sampler gNearestSampler;
