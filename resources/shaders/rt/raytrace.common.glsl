struct hitPayload {
    vec3 hitColor;
    bool hitSomething;
};

struct SceneElement {
    uint mappedIndex;
};

struct Vertex {
    vec4 inPosition;
    vec3 inColor;
    vec3 inNormal;
    vec2 inUV;
};