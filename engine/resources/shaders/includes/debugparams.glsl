#define DEBUG_GBUFFER_DISABLED 0
#define DEBUG_GBUFFER_ALBEDO 1
#define DEBUG_GBUFFER_POSITION 2
#define DEBUG_GBUFFER_DEPTH 3
#define DEBUG_GBUFFER_NORMAL 4
#define DEBUG_GBUFFER_METALLIC_ROUGHNESS 5
#define DEBUG_GBUFFER_EMISSIVE 6
#define DEBUG_GBUFFER_RANDOMNESS 7
#define DEBUG_GBUFFER_TANGENT 8
#define DEBUG_GBUFFER_LIGHTING 9
#define DEBUG_GBUFFER_MOTION 10
#define DEBUG_GBUFFER_MOMENTS 11
#define DEBUG_GBUFFER_ENTITYID 12
#define DEBUG_GBUFFER_NOISY_LIGHTING 13
#define DEBUG_POST_TEMPORAL_DENOISE 14
#define DEBUG_VARIANCE 15
#define DEBUG_POST_FIREFLY_REJECTION 16

#define DEBUG_VISIBILITY_BUFFER_FIRST 17
#define DEBUG_VISIBILITY_BUFFER_TRIANGLES 17
#define DEBUG_VISIBILITY_BUFFER_CLUSTERS 18
#define DEBUG_VISIBILITY_BUFFER_LODS 19
#define DEBUG_VISIBILITY_BUFFER_SCREEN_ERROR 20
#define DEBUG_VISIBILITY_BUFFER_LAST 20 // increment when adding new values!

#define DEBUG_OPTIONS_SET(SetID)                                                                                       \
layout(set = SetID, binding = 0) uniform Debug {                                                                       \
    uint gBufferType;                                                                                                  \
} debug;
