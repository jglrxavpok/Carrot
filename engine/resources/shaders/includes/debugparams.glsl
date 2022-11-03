#define DEBUG_GBUFFER_DISABLED 0
#define DEBUG_GBUFFER_ALBEDO 1
#define DEBUG_GBUFFER_POSITION 2
#define DEBUG_GBUFFER_DEPTH 3
#define DEBUG_GBUFFER_NORMAL 4
#define DEBUG_GBUFFER_METALLIC_ROUGHNESS 5
#define DEBUG_GBUFFER_EMISSIVE 6

#define DEBUG_OPTIONS_SET(SetID)                                                                                       \
layout(set = SetID, binding = 0) uniform Debug {                                                                       \
    uint gBufferType;                                                                                                  \
} debug;
