module lighting;
import base;

__include lighting.pbr;

public static constexpr float MaxLightDistance = 5000.0f;

public struct LightingPassUniforms {
    public uint frameCount;
    public uint frameWidth;
    public uint frameHeight;
    public bool hasTLAS;
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
                return clamp((force - getOuterCutoffCosAngle()) / cutoffRange, 0, 1);
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
}