module gbuffer;
import base;

__include gbuffer.io;
__include gbuffer.utils;


[Flags]
public enum IntProperties : uint32_t {
    None = 0,
    RayTracedLighting,
    NegativeViewNormalZ,
    NegativeViewTangentZ,
    NegativeViewBitangentZ,
}

/**
    "Fat" version of the gbuffer, should be the version used by shaders
*/
public struct GBuffer {
    public vec4 albedo;
    public vec3 viewPosition;
    public mat3 viewTBN;

    public IntProperties intProperty;
    public uvec4 entityID;

    public float metallicness;
    public float roughness;

    public vec3 emissiveColor;
    public float2 motionVector; // motion vector, in UV space

    public float readDepth; // depth of this pixel, only valid when *reading* from the gbuffer
    public bool readIsSky; // is this pixel a sky pixel, only valid when *reading* from the gbuffer

    public __init() {
        albedo = vec4(0.0);
        viewPosition = vec3(0.0);
        viewTBN = mat3(
                        vec3(1.0, 0.0, 0.0),
                        vec3(0.0, 1.0, 0.0),
                        vec3(0.0, 0.0, 1.0));

        intProperty = IntProperties::None;
        entityID = uvec4(0);
        metallicness = 0.0;
        roughness = 0.0;

        emissiveColor = vec3(0.0);
        motionVector = float2(0.0);
        readDepth = 0.0f/0.0f;
        readIsSky = false;
    }
};

/**
    Encoded version of the GBuffer: to reduce VRAM usage and render target count,
    some render targets are merged together
*/
public struct EncodedGBuffer {

    public vec4 color;
    public vec4 viewPosition;
    public vec4 viewNormalTangent;
    public uint32_t intProperty;
    public uvec4 entityID;
    public vec4 metallicRoughnessVelocityXY;
    public float3 emissive;

    public __init() {
        color = vec4(0.0);
        viewPosition = vec4(0.0);
        viewNormalTangent = vec4(0.0);
        intProperty = 0;
        entityID = uvec4(0);
        metallicRoughnessVelocityXY = vec4(0.0);
        emissive = float3(0.0);
    }

    public __init(GBuffer gbuffer) {
        color = gbuffer.albedo;
        viewPosition = vec4(gbuffer.viewPosition, 1 /*not sky*/);
        viewNormalTangent = vec4(1);
        intProperty = gbuffer.intProperty;
        entityID = gbuffer.entityID;
        metallicRoughnessVelocityXY = vec4(gbuffer.metallicness, gbuffer.roughness, gbuffer.motionVector.xy);

        emissive = gbuffer.emissiveColor;

        bool nSign = false;
        bool tSign = false;
        bool bSign = false;
        {
            compressTBN(gbuffer.viewTBN, viewNormalTangent, nSign, tSign, bSign);
            if (nSign) {
                intProperty = intProperty | IntProperties::NegativeViewNormalZ;
            }
            if (tSign) {
                intProperty = intProperty | IntProperties::NegativeViewTangentZ;
            }
            if (bSign) {
                intProperty = intProperty | IntProperties::NegativeViewBitangentZ;
            }
        }
    }

}