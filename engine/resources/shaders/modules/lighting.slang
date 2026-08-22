module lighting;
import glsl;
import base;
import raytracing_capabilities;
import vertex_buffers;
import rng;
import materials;

__include lighting.pbr;

public static constexpr float MaxLightDistance = 5000.0f;

public struct LightingPassUniforms {
    public uint frameCount;
    public uint frameWidth;
    public uint frameHeight;
    public bool hasTLAS;

    public property uint2 frameDimensions {
        get {
            return uint2(frameWidth, frameHeight);
        }
    }
}

[Flags]
public enum LightFlags: uint8_t {
    None = 0,
    Enabled,
}

public enum LightType: uint8_t {
    Point,
    Directional,
    Spot,
}

public struct Light {
    public float3 color;
    private LightFlags flags;
    public LightType type;
    public float intensity;

    private float3 v0; // point: position, spot: position, directional: direction
    private float3 v1; // point: attenuation factors, spot: direction, directional: nothing
    private float2 v2; // point: nothing, spot: cutoff angles, directional: nothing

    public bool isEnabled() {
        return (flags & LightFlags::Enabled) != 0;
    }

    public float3 getPosition() {
        return v0;
    }

    /**
     Valid only for directional and spot lights.
     */
    public float3 getDirection() {
        switch(type) {
            default: return 0;

            case LightType::Point:
                return float3(0);

            case LightType::Directional:
                return v0;

            case LightType::Spot:
                return v1;
        }
    }

    /**
     Valid only for point lights
    */
    public float3 getAttenuationFactors() {
        switch(type) {
            default: return 0;

            case LightType::Point:
                return v1;

            case LightType::Directional:
                return float3(0);

            case LightType::Spot:
                return float3(0);
        }
    }

    /**
     Valid only for spot lights
    */
    public float getCutoffCosAngle() {
        switch(type) {
            default: return 0;

            case LightType::Point:
                return 0;

            case LightType::Directional:
                return 0;

            case LightType::Spot:
                return v2.x;
        }
    }

    /**
     Valid only for spot lights
    */
    public float getOuterCutoffCosAngle() {
        switch(type) {
            default: return 0;

            case LightType::Point:
                return 0;

            case LightType::Directional:
                return 0;

            case LightType::Spot:
                return v2.y;
        }
    }

    public float3 computeContribution(float3 worldPos, float3 normal) {
        float shadowing = 1.0f;
        switch(type) {
            default: return 0;

            case LightType::Point: {
                const float distance = length(worldPos - getPosition());
                const float inclinaisonFactor = 1.0f;//abs(dot(normal, point2light / distance));
                const float3 factors = getAttenuationFactors();
                const float attenuation = factors.x + factors.y * distance + factors.z * distance * distance;
                return max(0, 1.0f / attenuation) * inclinaisonFactor;
            }

            case LightType::Directional: {
                return max(0, dot(getDirection(), normal));
            }

            case LightType::Spot: {
                const float3 point2light = normalize(getPosition() - worldPos);
                const float3 lightDirection = -getDirection();
                const float force = dot(point2light, lightDirection);
                const float cutoffRange = getOuterCutoffCosAngle() - getCutoffCosAngle();
                return clamp((getOuterCutoffCosAngle() - force) / cutoffRange, 0, 1);
            }
        }

        return shadowing * color * intensity;
    }

    public float3 getLightPositionRelativeTo(float3 worldPos) {
        switch(type) {
            default: return 0;

            case LightType::Point:
            case LightType::Spot:
            {
                return getPosition() - worldPos;
            }

            case LightType::Directional: {
                return getDirection() * MaxLightDistance;
            }
        }
    }
}

public struct LightArray {
    public float3 ambientColor;
    public uint lightCount;
    public float3 fogColor;
    public float fogDistance;
    public float fogDepth;
    public Light[] lights;
}

public struct ActiveLights {
    public uint count;
    public uint[] indices;
}

public struct EmissiveMesh {
    Vertex *vertices;
    uint32_t *indices;
    uint32_t triangleCount;
    float4x4 transform;
    uint32_t materialIndex;
}

public struct Lighting {
    public GLSLShaderStorageBuffer<LightArray, ScalarDataLayout> lightArray;

    public GLSLShaderStorageBuffer<ActiveLights> activeLights;

    public StructuredBuffer<EmissiveMesh, ScalarDataLayout> emissiveMeshes;
    public StructuredBuffer<uint, ScalarDataLayout> activeEmissiveMeshes;
    public StructuredBuffer<uint, ScalarDataLayout> activeEmissiveMeshesCount; // size of 1

    public float3 sampleLights(out float lightPDF, in IRaytracingCapabilities config, PbrInputs pbr, float3 worldPos, float3 normal, float3 tangent, float metallic, float roughness) {
        float3 contribution = 0;
        lightPDF = 1.0f;
        for(uint i = 0; i < activeLights.count; i++) {
            uint lightIndex = activeLights.indices[i];
            
            const Light l = lightArray.lights[lightIndex];
            
            if(!l.isEnabled()) {
                continue;
            }

            const float3 L = l.getLightPositionRelativeTo(worldPos);
            const float3 smallOffset = normalize(L) * 0.001f;
            const float visibility = float(config.checkVisibility(worldPos+smallOffset, worldPos+L));
            float3 singleLightContribution = l.computeContribution(worldPos, normal) /* cos term already in computeLightContribution */;
            singleLightContribution *= l.intensity * l.color;

            pbr.L = normalize(L);
            pbr.H = normalize(pbr.L + pbr.V);

            float NdotL = dot(pbr.N, pbr.L);
            if(NdotL < 0) {
                continue;
            }

            pbr.NdotH = dot(pbr.N, pbr.H);
            pbr.NdotL = abs(NdotL);
            pbr.HdotL = dot(pbr.H, pbr.L);
            pbr.HdotV = dot(pbr.H, pbr.V);

            const float3 brdf = glTF_BRDF_WithoutImportanceSampling(pbr);
            if(isnan(brdf.x)) {
                return float3(10, 0, 0);
            }

            contribution += brdf * visibility * singleLightContribution;
        }
        return contribution;
    }

    /**
    * Samples emissive meshes present in scene to compute how much light 'worldPos' receives
    */
    public float3 sampleEmissives(out float pdf, inout RandomSampler rng, in MaterialSystem materials, in IRaytracingCapabilities config, PbrInputs pbr, float3 worldPos, float3 normal) {
        const uint count = activeEmissiveMeshesCount[0];
        pdf = 1.0f;
        if (count == 0)
            return float3(0,0,0);

        // pick a random mesh and triangle        
        const uint meshIndex = min(uint(rng.next(materials) * count), count-1);
        const EmissiveMesh mesh = emissiveMeshes[activeEmissiveMeshes[meshIndex]];
        const uint triangleIndex = min(uint(rng.next(materials) * mesh.triangleCount), mesh.triangleCount-1);

        // compute emissivity on the triangle
        const uint indexA = mesh.indices[triangleIndex * 3 + 0];
        const uint indexB = mesh.indices[triangleIndex * 3 + 1];
        const uint indexC = mesh.indices[triangleIndex * 3 + 2];
        const Vertex vA = mesh.vertices[indexA];
        const Vertex vB = mesh.vertices[indexB];
        const Vertex vC = mesh.vertices[indexC];

        float u = rng.next(materials);
        float v = rng.next(materials);

        if (u + v > 1) {
            u = 1 - u;
            v = 1 - v;
        }

        const float w = 1 - u - v;
        const float3 posA = (mesh.transform * vA.getPos()).xyz;
        const float3 posB = (mesh.transform * vB.getPos()).xyz;
        const float3 posC = (mesh.transform * vC.getPos()).xyz;
        const float3 posOnTriangle = posA * u + posB * v + posC * w;
        const float2 uv = vA.getUV() * u + vB.getUV() * v + vC.getUV() * w;
        const float3 normalOnLight = transpose(inverse(float3x3(mesh.transform))) * (vA.getNormal() * u + vB.getNormal() * v + vC.getNormal() * w);

        const Material material = materials.materials[NonUniformResourceIndex(mesh.materialIndex)];
        const float3 emissiveLight = material.Compute(materials, materials.linearSampler, uv).emissive;

        // compute PDF (importance sampling)
        const float triangleArea = length(cross((posA - posB).xyz, (posA - posC).xyz)) * 0.5f;
        pdf = 1.0f / float(count);
        pdf *= 1.0f / float(mesh.triangleCount);
        pdf *= 1.0f / float(triangleArea);

        const float3 L = posOnTriangle - worldPos;

        // check visibility
        const float3 smallOffset = normalize(L) * 0.0001f;
        const float visibility = float(config.checkVisibility(worldPos + smallOffset, worldPos + L - smallOffset));

        pbr.L = normalize(L);
        pbr.H = normalize(pbr.L + pbr.V);

        float NdotL = dot(pbr.N, pbr.L);
        if (NdotL < 0) {
            return float3(0,0,0);
        }

        pbr.NdotH = dot(pbr.N, pbr.H);
        pbr.NdotL = abs(NdotL);
        pbr.HdotL = dot(pbr.H, pbr.L);
        pbr.HdotV = dot(pbr.H, pbr.V);

        // TODO: test without this brdf but only G factor
        /*const float3 brdf = glTF_BRDF_WithImportanceSampling(pbr);
        if (isnan(brdf.x)) {
            return float3(0, 0, 0);
        }*/

        const float brdf = dot(normal, normalize(L)) * dot(-normalize(L), normalOnLight);

        const float d = distance(posOnTriangle, worldPos);
        if (d > 0.0001f) {
            return brdf * visibility * emissiveLight / (d * d);
        }
        return float3(0,0,0);
    }
}