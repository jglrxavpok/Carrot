#include "gbuffer.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out uint intProperty;
layout(location = 4) out uvec4 entityID;
layout(location = 5) out vec4 metallicRoughness;
layout(location = 6) out vec4 emissive;
layout(location = 7) out vec4 outTangent;
// TODO: motion vectors

struct GBuffer {
    vec4 albedo;
    vec3 viewPosition;
    vec3 viewNormal;
    vec3 viewTangent;

    uint intProperty;
    uvec4 entityID;

    float metallicness;
    float roughness;

    vec3 emissiveColor;
};

GBuffer initGBuffer() {
    GBuffer gbuffer;

    gbuffer.albedo = vec4(0.0);
    gbuffer.viewPosition = vec3(0.0);
    gbuffer.viewNormal = vec3(0.0, -1.0, 0.0);
    gbuffer.viewTangent = vec3(1.0, 0.0, 0.0);

    gbuffer.intProperty = 0;
    gbuffer.entityID = uvec4(0);
    gbuffer.metallicness = 0.0;
    gbuffer.roughness = 0.0;

    gbuffer.emissiveColor = vec3(0.0);

    return gbuffer;
}

void outputGBuffer(in GBuffer o) {
    outColor = o.albedo;
    outViewPosition = vec4(o.viewPosition, 1.0);
    outNormal = vec4(o.viewNormal, 0.0);
    outTangent = vec4(o.viewTangent, 0.0);
    intProperty = o.intProperty;
    entityID = o.entityID;
    metallicRoughness = vec4(o.metallicness, o.roughness, 0.0, 0.0);
    emissive = vec4(o.emissiveColor, 1.0);
}