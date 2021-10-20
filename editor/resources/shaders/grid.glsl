layout(push_constant) uniform GridData {
    layout(offset = 0) vec4 color; // RGBA
    ivec2 screenSize; // in pixels
    float lineWidth; // in units
    float cellSize; // in units
    float size; // in units
} grid;