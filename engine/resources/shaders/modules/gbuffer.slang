module gbuffer;
import glsl;
import base;

__include gbuffer.io;
__include gbuffer.utils;

/**
    "Fat" version of the gbuffer, should be the version used by shaders
*/
public struct GBuffer {
    public vec4 albedo;
    public vec3 viewPosition;
    public mat3 worldTBN;

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
        worldTBN = mat3(
                        vec3(1.0, 0.0, 0.0),
                        vec3(0.0, 1.0, 0.0),
                        vec3(0.0, 0.0, 1.0));

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
    public uint16_t4 worldNormalTangent;
    public uvec4 entityID;
    public vec4 metallicRoughnessVelocityXY;
    public float3 emissive;

    public __init() {
        color = vec4(0.0);
        viewPosition = vec4(0.0);
        worldNormalTangent = uint16_t4(0);
        entityID = uvec4(0);
        metallicRoughnessVelocityXY = vec4(0.0);
        emissive = float3(0.0);
    }

    public __init(GBuffer gbuffer) {
        color = gbuffer.albedo;
        viewPosition = vec4(gbuffer.viewPosition, 1 /*not sky*/);
        worldNormalTangent = vec4(1);
        entityID = gbuffer.entityID;
        metallicRoughnessVelocityXY = vec4(gbuffer.metallicness, gbuffer.roughness, gbuffer.motionVector.xy);

        emissive = gbuffer.emissiveColor;

        compressTBN(gbuffer.worldTBN, worldNormalTangent);
    }

}