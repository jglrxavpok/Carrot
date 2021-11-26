struct DrawData {
    uint materialIndex;

    uint uuid0;
    uint uuid1;
    uint uuid2;
    uint uuid3;
};

layout(push_constant) uniform DrawDataPushConstant {
    DrawData drawData[];
} drawDataPush;
