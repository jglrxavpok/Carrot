#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "includes/gbuffer.glsl"

layout(constant_id = 0) const uint MAX_TEXTURES = 1;
layout(constant_id = 1) const uint MAX_MATERIALS = 1;

struct MaterialData {
    uint textureIndex;
    bool ignoresInstanceColor;
};

layout(set = 0, binding = 0) uniform texture2D textures[MAX_TEXTURES];
layout(set = 0, binding = 1) uniform sampler linearSampler;

// unused, but required for GBuffer pipelines
layout(set = 0, binding = 2) buffer MaterialBuffer {
    MaterialData materials[MAX_MATERIALS];
};

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec3 viewPosition;
layout(location = 4) in vec3 viewNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out uint intProperty;

void main() {
    vec4 texColor = texture(sampler2D(textures[0], linearSampler), uv);
    if(texColor.a < 0.01) {
        discard;
    }
    outColor = texColor * fragColor;
    outViewPosition = vec4(viewPosition, 1.0);
    outNormal = vec4(viewNormal, 1.0);
    intProperty = IntPropertiesRayTracedLighting;
}