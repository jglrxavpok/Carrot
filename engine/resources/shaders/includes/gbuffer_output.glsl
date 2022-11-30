#include "gbuffer.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outNormalTangent;
layout(location = 3) out uint intProperty;
layout(location = 4) out uvec4 entityID;
layout(location = 5) out vec4 metallicRoughness;
layout(location = 6) out vec4 emissive;
layout(location = 7) out vec4 outVelocity;

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
    gbuffer.motionVector = vec3(0.0);

    return gbuffer;
}

void outputGBuffer(in GBuffer o) {
    outColor = o.albedo;
    outViewPosition = vec4(o.viewPosition, 1.0);
    vec3 n = normalize(o.viewNormal);
    vec3 t = normalize(o.viewTangent);
    outNormalTangent = vec4(n.xy, t.xy);

    uint nSign = 0;
    if(n.z < 0)
    {
        nSign = IntPropertiesNegativeNormalZ;
    }

    uint tSign = 0;
    if(t.z < 0)
    {
        tSign = IntPropertiesNegativeTangentZ;
    }

    intProperty = o.intProperty | nSign | tSign;
    entityID = o.entityID;
    metallicRoughness = vec4(o.metallicness, o.roughness, 0.0, 0.0);
    emissive = vec4(o.emissiveColor, 1.0);
    outVelocity = vec4(o.motionVector, 0.0);
}