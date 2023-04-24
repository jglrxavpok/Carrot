#extension GL_EXT_scalar_block_layout : enable

struct DrawData {
    uint materialIndex;

    uint uuid0;
    uint uuid1;
    uint uuid2;
    uint uuid3;
};

#define DEFINE_PER_DRAW_PUSH()                                                                                          \
layout(push_constant) uniform DrawDataPushConstant {                                                                    \
    DrawData drawData[];                                                                                                \
} perDrawData;


#define DEFINE_PER_DRAW_BUFFER(setID)                                                                                   \
layout(set = setID, binding = 0, scalar) buffer PerDrawData {                                                           \
    DrawData drawData[];                                                                                                \
} perDrawData;                                                                                                          \
                                                                                                                        \
layout(set = setID, binding = 1) uniform PerDrawDataOffsets {                                                           \
    uint offset;                                                                                                        \
} perDrawDataOffsets;
