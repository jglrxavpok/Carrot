#ifndef _BRDF_GLSL
#define _BRDF_GLSL
#include <includes/math.glsl>
#include <includes/rng.glsl>

// BRDFs based on glTF spec: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf

// Implementation from https://github.com/SaschaWillems/Vulkan-glTF-PBR (MIT):

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec3 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal)
{
    // Maps a 2D point to a hemisphere with spread based on roughness
    float alpha = roughness * roughness;
    float phi = 2.0 * M_PI * Xi.x; // removed random factor here as we are not on CPU
    float cosTheta = sqrt(max(0.0, (1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y)));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Tangent space
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, normal));
    vec3 tangentY = normalize(cross(normal, tangentX));

    // Convert to world Space
    return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

struct PbrInputs {
    float alpha;
    float metallic;
    vec3 baseColor;
    vec3 V; // -incoming ray
    vec3 L; // direction to light
    vec3 N; // surface normal
    vec3 H; // half vector
    float NdotH;
    float NdotL;
    float HdotL;
    float HdotV;
    float NdotV;
};

// D and G functions based on https://github.com/SaschaWillems/Vulkan-glTF-PBR
// Under MIT license
float D(in PbrInputs pbr) {
    float roughnessSq = pbr.alpha * pbr.alpha;
    float f = (pbr.NdotH * roughnessSq - pbr.NdotH) * pbr.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

float G(in PbrInputs pbr) {
    float NdotL = pbr.NdotL;
    float NdotV = pbr.NdotV;
    float r = pbr.alpha;
    if(NdotL <= 0) return 0.0;
    if(NdotV <= 0) return 0.0;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// ==== End of implementation from Vulkan-glTF-PBR

vec3 glTF_BRDF_WithImportanceSampling(in PbrInputs pbr) {
    vec3 c_diff = mix(pbr.baseColor, vec3(0), pbr.metallic);
    vec3 f0 = mix(vec3(0.04), pbr.baseColor, pbr.metallic);
    float alpha = pbr.alpha;
    vec3 F = f0 + (1 - f0) * pow(1 - abs(pbr.HdotV), 5);

    vec3 f_diffuse = (1 - F) * M_INV_PI * c_diff;
    // no D term: SaschaWillems' PBR renderer does not use it for its BRDF LUT generation
    // According to https://google.github.io/filament/Filament.md.html#lighting/imagebasedlights/distantlightprobes/specularbrdfintegration/discretedomain
    //  due to the usage of importance sampling, the D term gets cancelled
    vec3 f_specular = F * G(pbr) / (4 * abs(pbr.NdotV) * abs(pbr.NdotL));
    return f_diffuse + f_specular;
}

vec3 glTF_BRDF_WithoutImportanceSampling(in PbrInputs pbr) {
    vec3 c_diff = mix(pbr.baseColor, vec3(0), pbr.metallic);
    vec3 f0 = mix(vec3(0.04), pbr.baseColor, pbr.metallic);
    float alpha = pbr.alpha;
    vec3 F = f0 + (1 - f0) * pow(1 - abs(pbr.HdotV), 5);

    vec3 f_diffuse = (1 - F) * M_INV_PI * c_diff;
    vec3 f_specular = F * D(pbr) * G(pbr) / (4 * abs(pbr.NdotV) * abs(pbr.NdotL));
    return f_diffuse + f_specular;
}

void computeDotProducts(inout PbrInputs inputs) {
    inputs.NdotH = dot(inputs.N, inputs.H);
    inputs.NdotL = dot(inputs.N, inputs.L);
    inputs.HdotL = dot(inputs.H, inputs.L);
    inputs.HdotV = dot(inputs.H, inputs.V);
    inputs.NdotV = abs(dot(inputs.N, inputs.V));
}

vec3 importanceSampleDirectionForReflection(inout RandomSampler rng, vec3 incomingRay, vec3 normal, float roughness) {
    vec3 direction;
    if(roughness >= 10e-4f) { // maybe a subgroup operation would be better?
        vec3 lambertianDirection;
        float pdf;
        vec3 microfacetNormal = importanceSample_GGX(vec2(sampleNoise(rng), sampleNoise(rng)), roughness, normal);
        direction = reflect(incomingRay, microfacetNormal);
    } else {
        direction = reflect(incomingRay, normal);
    }
    return direction;
}
#endif // _BRDF_GLSL