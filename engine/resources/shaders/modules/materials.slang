module materials;
import base;

/**
    Values for a given material, after sampling, blending, etc.
*/
public struct MaterialOutput {
    public float4 albedo;
    public float3 emissive;
    public float3 normal; // normal in texture space (+Z up / on top of texture)
    public float metallic;
    public float roughness;
}

public struct Material {
    public vec4 baseColor;

    public vec3 emissiveColor;
    public uint emissive;

    public vec2 metallicRoughnessFactor;

    public uint albedo;
    public uint normalMap;
    public uint metallicRoughness;

    /**
        Samples and computes the outputs for this material (ie color, normal, etc)
        If special material types need to be added, this is where it should be done.
    */
    public MaterialOutput Compute(in MaterialSystem materials, in SamplerState sampler, float2 uv) {
        MaterialOutput output;

        output.albedo = baseColor;
        if (albedo >= 0) {
            output.albedo *= materials.Sample(NonUniformResourceIndex(albedo), sampler, uv);
        }

        output.emissive = emissiveColor;
        if (emissive >= 0) {
            output.emissive *= materials.Sample(NonUniformResourceIndex(emissive), sampler, uv).rgb;
        }

        output.metallic = metallicRoughnessFactor.x;
        output.roughness = metallicRoughnessFactor.y;
        if (metallicRoughness >= 0) {
            const vec2 metallicRoughnessValues = materials.Sample(NonUniformResourceIndex(metallicRoughness), sampler, uv).xy;
            output.metallic *= metallicRoughnessValues.x;
            output.roughness *= metallicRoughnessValues.y;
        }

        output.normal = vec3(0, 0, 1);
        if (normalMap >= 0) {
            output.normal = normalize(materials.Sample(NonUniformResourceIndex(normalMap), sampler, uv).xyz * 2 - 1);
        }

        return output;
    }
};

public struct GlobalTextures {
    uint blueNoises[64];
    uint dithering;
}

public struct MaterialSystem {
    public StructuredBuffer<Material, ScalarDataLayout> materials;
    public ConstantBuffer<Texture2D[]> textures;
    public SamplerState linearSampler;
    public SamplerState nearestSampler;
    public ConstantBuffer<GlobalTextures, ScalarDataLayout> globalTextures;

    public float4 SampleAlbedo(in Material mat, in SamplerState sampler, float2 uv) {
        return textures[NonUniformResourceIndex(mat.albedo)].Sample(sampler, uv);
    }

    public float4 Sample(uint textureIndex, in SamplerState sampler, float2 uv) {
        return textures[NonUniformResourceIndex(textureIndex)].Sample(sampler, uv);
    }

    public float Dither(uvec2 coords) {
        const uint DitherSize = 8;
        const vec2 ditherUV = vec2(coords % DitherSize) / DitherSize;
        return textures[globalTextures.dithering].Sample(nearestSampler, ditherUV).r;
    }
}