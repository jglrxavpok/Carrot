layout(set = 0, binding = 0) uniform usampler2D entityIDTexture;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant {
    uvec4 selectedEntity;
} selection;


void main() {
    bool isSelectedAtCenter = false;
    bool isSelectedInWindow = false;
    const vec2 textureDimensions = textureSize(entityIDTexture, 0);
    const int radius = 2;
    float maxDistanceSq = 100000;
    for(int dx = -radius; dx <= radius; dx++) {
        for(int dy = -radius; dy <= radius; dy++) {
            const vec2 dUV = vec2(dx, dy) / textureDimensions;
            const uvec4 entityAtPixel = texture(entityIDTexture, uv + dUV);
            const bool entityMatches = entityAtPixel == selection.selectedEntity;

            if(entityMatches)
            {
                float currentDistanceSq = dot(vec2(dx, dy), vec2(dx, dy));
                if(currentDistanceSq < maxDistanceSq) {
                    maxDistanceSq = currentDistanceSq;
                }
                isSelectedInWindow = true;
                if(dx == 0 && dy == 0) {
                    isSelectedAtCenter = true;
                }
            }

        }
    }

    if(!isSelectedAtCenter && isSelectedInWindow) {
        outColor = vec4(1.0f, 0x81 / 255.0f, 0x46 / 255.0f, 1.0f);
    } else {
        discard;
    }
}
