implementing lighting;


public struct PbrInputs {
    public float alpha;
    public float metallic;
    public float3 baseColor;
    public float3 V; // -incoming ray
    public float3 L; // direction to light
    public float3 N; // surface normal
    public float3 H; // half vector
    public float NdotH;
    public float NdotL;
    public float HdotL;
    public float HdotV;
    public float NdotV;

    [mutating]
    void computeDotProducts() {
        NdotH = dot(N, H);
        NdotL = dot(N, L);
        HdotL = dot(H, L);
        HdotV = dot(H, V);
        NdotV = abs(dot(N, V));
    }
};

// D and G functions based on https://github.com/SaschaWillems/Vulkan-glTF-PBR
// Under MIT license
float D(in PbrInputs pbr) {
    float roughnessSq = pbr.alpha * pbr.alpha;
    float f = (pbr.NdotH * roughnessSq - pbr.NdotH) * pbr.NdotH + 1.0;
    return roughnessSq / (M_PIf * f * f);
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

public float3 glTF_BRDF_WithImportanceSampling(in PbrInputs pbr) {
    float3 c_diff = lerp(pbr.baseColor, float3(0), pbr.metallic);
    float3 f0 = lerp(float3(0.04), pbr.baseColor, pbr.metallic);
    float alpha = pbr.alpha;
    float3 F = f0 + (1 - f0) * pow(1 - abs(pbr.HdotV), 5);

    float3 f_diffuse = (1 - F) * M_INV_PIf * c_diff;
    // no D term: SaschaWillems' PBR renderer does not use it for its BRDF LUT generation
    // According to https://google.github.io/filament/Filament.md.html#lighting/imagebasedlights/distantlightprobes/specularbrdfintegration/discretedomain
    //  due to the usage of importance sampling, the D term gets cancelled
    float3 f_specular = F * G(pbr) / (4 * abs(pbr.NdotV) * abs(pbr.NdotL));
    return f_diffuse + f_specular;
}

public float3 glTF_BRDF_WithoutImportanceSampling(in PbrInputs pbr) {
    float3 c_diff = lerp(pbr.baseColor, float3(0), pbr.metallic);
    float3 f0 = lerp(float3(0.04), pbr.baseColor, pbr.metallic);
    float alpha = pbr.alpha;
    float3 F = f0 + (1 - f0) * pow(1 - abs(pbr.HdotV), 5);

    float3 f_diffuse = (1 - F) * M_INV_PIf * c_diff;
    float3 f_specular = F * D(pbr) * G(pbr) / (4 * abs(pbr.NdotV) * abs(pbr.NdotL));
    return f_diffuse + f_specular;
}