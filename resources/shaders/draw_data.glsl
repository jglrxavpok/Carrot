struct DrawData {
    uint materialIndex;
};

layout(push_constant) uniform DrawDataPushConstant {
    DrawData drawData[];
};
