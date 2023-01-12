#include "gbuffer.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outViewPosition;
layout(location = 2) out vec4 outViewNormalTangent;
layout(location = 3) out uint intProperty;
layout(location = 4) out uvec4 entityID;
layout(location = 5) out vec4 metallicRoughnessVelocityXY;
layout(location = 6) out vec4 emissiveVelocityZ;
layout(location = 7) out vec4 outTangentSpaceNormalTangent;

GBuffer initGBuffer(mat4 modelview) {
    GBuffer gbuffer;

    gbuffer.albedo = vec4(0.0);
    gbuffer.viewPosition = vec3(0.0);
    gbuffer.viewTBN = mat3(modelview) * mat3(
                vec3(1.0, 0.0, 0.0),
                vec3(0.0, 1.0, 0.0),
                vec3(0.0, 0.0, 1.0));

    gbuffer.intProperty = 0;
    gbuffer.entityID = uvec4(0);
    gbuffer.metallicness = 0.0;
    gbuffer.roughness = 0.0;

    gbuffer.emissiveColor = vec3(0.0);
    gbuffer.motionVector = vec3(0.0);

    return gbuffer;
}

void outputGBuffer(in GBuffer o, mat4 modelview) {
    outColor = o.albedo;
    outViewPosition = vec4(o.viewPosition, 1.0);

    bool nSign = false;
    bool tSign = false;
    bool bSign = false;
    {
        compressTBN(o.viewTBN, outViewNormalTangent, nSign, tSign, bSign);

        if(nSign) {
            o.intProperty |= IntPropertiesNegativeViewNormalZ;
        }
        if(tSign) {
            o.intProperty |= IntPropertiesNegativeViewTangentZ;
        }
        if(bSign) {
            o.intProperty |= IntPropertiesNegativeViewBitangent;
        }
    }

    intProperty = o.intProperty;
    entityID = o.entityID;
    metallicRoughnessVelocityXY = vec4(o.metallicness, o.roughness, o.motionVector.x, o.motionVector.y);
    emissiveVelocityZ = vec4(o.emissiveColor, o.motionVector.z);
}