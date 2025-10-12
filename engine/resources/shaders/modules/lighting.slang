module lighting;
import base;
import raytracing_capabilities;

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
            } break;

            case LightType::Directional: {
                return max(0, dot(getDirection(), normal));
            } break;

            case LightType::Spot: {
                const float3 point2light = normalize(getPosition() - worldPos);
                const float3 lightDirection = -getDirection();
                const float force = dot(point2light, lightDirection);
                const float cutoffRange = getOuterCutoffCosAngle() - getCutoffCosAngle();
                return clamp((getOuterCutoffCosAngle() - force) / cutoffRange, 0, 1);
            } break;
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
            } break;

            case LightType::Directional: {
                return getDirection() * MaxLightDistance;
            } break;
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

public struct Lighting {
    public GLSLShaderStorageBuffer<LightArray, ScalarDataLayout> lightArray;

    public GLSLShaderStorageBuffer<ActiveLights> activeLights;

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
}