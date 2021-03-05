struct hitPayload {
    vec3 hitColor;
    bool hitSomething;
};

struct SceneElement {
    uint mappedIndex;
    mat4 transform;
};

struct Vertex {
    vec4 pos;
    vec3 color;
    vec3 normal;
    vec2 uv;
};