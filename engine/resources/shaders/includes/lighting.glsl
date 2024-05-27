#include <includes/math.glsl>

struct SurfaceIntersection {
    vec3 position;
    uint materialIndex;
    vec3 surfaceNormal;
    vec3 surfaceTangent;
    vec3 surfaceColor;
    vec2 uv;
    bool hasIntersection;
};

// from Physically Based Rendering
float powerHeuristic(int nf, float fPDF, int ng, float gPDF) {
    float f = nf * fPDF;
    float g = ng * gPDF;
    return (f*f) / (f*f + g*g);
}

// ============= TANGENT SPACE ONLY =============
// based on https://www.pbr-book.org/3ed-2018/Reflection_Models#SinTheta
float cos2Theta(vec3 w) {
    return w.z * w.z;
}

float sin2Theta(vec3 w) {
    return max(0, 1-cos2Theta(w));
}

float cosTheta(vec3 w) {
    return sqrt(cos2Theta(w));
}

float sinTheta(vec3 w) {
    return sqrt(sin2Theta(w));
}

float tan2Theta(vec3 w) {
    return sin2Theta(w) / cos2Theta(w);
}

float tanTheta(vec3 w) {
    return sqrt(tan2Theta(w));
}

float cosPhi(vec3 w) {
    float sinTheta = sinTheta(w);
    return sinTheta == 0.0 ? 1 : clamp(w.x / sinTheta, -1, 1);
}

float sinPhi(vec3 w) {
    float sinTheta = sinTheta(w);
    return sinTheta == 0.0 ? 1 : clamp(w.y / sinTheta, -1, 1);
}

float cos2Phi(vec3 w) {
    float f = cosPhi(w);
    return f*f;
}

float sin2Phi(vec3 w) {
    float f = sinPhi(w);
    return f*f;
}

// https://github.com/mmp/pbrt-v3/blob/aaa552a4b9cbf9dccb71450f47b268e0ed6370e2/src/core/microfacet.h#L127
float roughnessToAlpha(float roughness) {
    /*roughness = max(roughness, float(1e-3f));
    float x = log(roughness);

    return max(0.001, 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x + 0.000640711f * x * x * x * x);*/
    return roughness*roughness;
}

// https://www.pbr-book.org/3ed-2018/Reflection_Models/Microfacet_Models
// distribution of microfacets
float ggxD(vec3 wh/*half vector*/, float alphax, float alphay) {
    float tan2Theta = tan2Theta(wh);
    if(isinf(tan2Theta)) return 0.0;
    float cos4Theta = cos2Theta(wh) * cos2Theta(wh);

    float e = tan2Theta * (cos2Phi(wh) / (alphax*alphax) + sin2Phi(wh) / (alphay*alphay));
    return 1.0 / (
    M_PI * alphax * alphay * cos4Theta * (1 + e) * (1 + e)
    );
}

float ggxLambda(vec3 w, float alphax, float alphay) {
    float absTanTheta = abs(tanTheta(w));
    if(isinf(absTanTheta)) return 0.0f;
    float alpha = sqrt(cos2Phi(w) * alphax * alphax + sin2Phi(w) * alphay * alphay);
    float alpha2Tan2Theta = (alpha * absTanTheta) * (alpha * absTanTheta);
    return (-1 + sqrt(1 + alpha2Tan2Theta)) / 2.0;
}

// masking-shadowing
float ggxG(vec3 wo, vec3 wi, float alphax, float alphay) {
    return 1 / (1 + ggxLambda(wo, alphax, alphay) + ggxLambda(wi, alphax, alphay));
}

float ggxG1(vec3 wo, vec3 wh, float alphax, float alphay) {
    return 1 / (1 + ggxLambda(wo, alphax, alphay));
}

vec2 concentricSampleDisk(inout RandomSampler rng) {
    vec2 u = vec2(sampleNoise(rng), sampleNoise(rng)) * 2.0 - 1.0;
    if(u.x == 0 && u.y == 0)
        return vec2(0.0);

    float theta = 0.0f;
    float r = 0.0f;

    if(abs(u.x) >= abs(u.y)) {
        r = u.x;
        theta = M_PI_OVER_4 * (u.y / u.x);
    } else {
        r = u.y;
        theta = M_PI_OVER_2 - M_PI_OVER_4 * (u.x / u.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

vec3 cosineSampleHemisphere(inout RandomSampler rng) {
    vec2 d = concentricSampleDisk(rng);
    float z = sqrt(max(0, 1 - dot(d, d)));
    return vec3(d.x, d.y, z);
}

vec3 schlickFresnel(vec3 f0, float lDotH) {
    return f0 + (vec3(1.0f, 1.0f, 1.0f) - f0) * pow(1.0f - lDotH, 5.0f);
}

vec3 computeF(vec3 wo, vec3 wi, float alphax, float alphay) {
    float cosThetaO = abs(cosTheta(wo));
    float cosThetaI = abs(cosTheta(wi));

    vec3 wh = wi + wo;
    // degenerate cases
    if(cosThetaI == 0.0 || cosThetaO == 0.0)
    return vec3(0.0);
    if(dot(wh, wh) == 0)
    return vec3(0.0);

    wh = normalize(wh);
    return vec3(ggxD(wh, alphax, alphay) * ggxG(wo, wi, alphax, alphay) / (4 * cosThetaI * cosThetaO));
}

vec3 sphericalDirection(float sinTheta, float cosTheta, float phi) {
    return vec3(
    sinTheta * cos(phi),
    sinTheta * sin(phi),
    cosTheta
    );
}

vec3 sampleWH(inout RandomSampler rng, vec3 wo, float alphax, float alphay) {
    vec3 wh;

    #if 0

    wh = normalize(cosineSampleHemisphere(rng)+wo);

    #else
    // WH sampling which does not account for self-shadowing microfacets
    float tan2Theta;
    float phi;
    if(alphax == alphay) {
        float logSample = log(1 - sampleNoise(rng));
        if(isinf(logSample)) logSample = 0.0f;
        tan2Theta = -alphax * alphax * logSample;
        phi = sampleNoise(rng) * 2 * M_PI;
    } else {
        float logSample = log(sampleNoise(rng));

        float u1 = sampleNoise(rng);
        phi = atan(alphay / alphax * tan(2 * M_PI * u1 + 0.5 * M_PI));
        if(u1 > 0.5f) {
            phi += M_PI;
        }

        float sinPhi = sin(phi);
        float cosPhi = cos(phi);
        float alphax2 = alphax * alphax;
        float alphay2 = alphay * alphay;
        tan2Theta = -logSample / (cosPhi * cosPhi / alphax2 + sinPhi * sinPhi / alphay2);
    }

    float cosTheta = 1.0 / sqrt(1.0 + tan2Theta);
    float sinTheta = sqrt(max(0, 1.0 - cosTheta * cosTheta));
    wh = sphericalDirection(sinTheta, cosTheta, phi);
    if(wo.z * wh.z < 0.0) {
        wh = -wh;
    }
    #endif
    return wh;
}

float computeGGX_PDF(vec3 wo, vec3 wh, float alphax, float alphay) {
    return (ggxD(wh, alphax, alphay) * /*ggxG1(wo, wh, alphax, alphay) **/ abs(cosTheta(wh)));//abs(dot(wo, wh)) / abs(cosTheta(wo)));
}

vec3 sampleGGXF(inout RandomSampler rng, float roughness, vec3 emitDirection, inout vec3 incidentDirection, inout float pdf) {
    float alphax = roughnessToAlpha(roughness);
    float alphay = alphax;

    vec3 wh = sampleWH(rng, emitDirection, alphax, alphay);
    if(dot(emitDirection, wh) < 0.0) return vec3(0.0f);
    incidentDirection = reflect(emitDirection, wh);
    if(emitDirection.z * incidentDirection.z < 0.0) return vec3(0.0f);

    pdf = computeGGX_PDF(emitDirection, wh, alphax, alphay) / (4 * dot(emitDirection, wh));

    return computeF(emitDirection, incidentDirection, alphax, alphay);
}

float computeLambertian_PDF(vec3 wo, vec3 wi) {
    if (wo.z * wi.z < 0) {
        return 0.0f;
    }

    return abs(wi.z) * M_INV_PI;
}

vec3 sampleLambertianF(inout RandomSampler rng, float roughness, vec3 emitDirection, inout vec3 incidentDirection, inout float pdf) {
    incidentDirection = cosineSampleHemisphere(rng);
    float woDotN = emitDirection.z;
    if(woDotN < 0) incidentDirection.z *= -1.0;
    pdf = computeLambertian_PDF(emitDirection, incidentDirection);

    return vec3(M_INV_PI);
}

vec3 sampleF(inout RandomSampler rng, float roughness, vec3 emitDirection, inout vec3 incidentDirection, inout float pdf) {
    float probabilityToSampleDiffuse = 0.5f;

    vec3 result;
    if(sampleNoise(rng) < probabilityToSampleDiffuse) {
        result = sampleLambertianF(rng, roughness, emitDirection, incidentDirection, pdf) / probabilityToSampleDiffuse;
    } else {
        result = sampleGGXF(rng, roughness, emitDirection, incidentDirection, pdf) / (1.0f-probabilityToSampleDiffuse);
    }
    return result;
}

// ============= END OF TANGENT SPACE ONLY =============


float computePointLight(vec3 worldPos, vec3 normal, uint lightIndex) {
    #define light lights.l[lightIndex]
    vec3 lightPosition = light.position;
    vec3 point2light = lightPosition-worldPos;

    float distance = length(point2light);
    float attenuation = light.constantAttenuation + light.linearAttenuation * distance + light.quadraticAttenuation * distance * distance;

    float inclinaisonFactor = 1.0;//abs(dot(normal, point2light / distance));

    return max(0, 1.0f / attenuation) * inclinaisonFactor;
    #undef light
}

float computeDirectionalLight(vec3 worldPos, vec3 normal, uint lightIndex) {
    #define light lights.l[lightIndex]
    vec3 lightDirection = normalize(light.direction);
    return max(0, dot(lightDirection, normal));
    #undef light
}

float computeSpotLight(vec3 worldPos, vec3 normal, uint lightIndex) {
    #define light lights.l[lightIndex]
    float cutoffCosAngle = light.cutoffCosAngle;
    float outerCosCutoffAngle = light.outerCosCutoffAngle;

    vec3 lightPosition = light.position;
    vec3 point2light = normalize(lightPosition-worldPos);
    vec3 lightDirection = -normalize(light.direction);
    float intensity = dot(point2light, lightDirection);
    float cutoffRange = outerCosCutoffAngle - cutoffCosAngle;
    return clamp((intensity - outerCosCutoffAngle) / cutoffRange, 0, 1);
    #undef light
}


/**
* Computes the direct contribution of the given light
*/
vec3 computeLightContribution(uint lightIndex, vec3 worldPos, vec3 normal) {
    #define light lights.l[lightIndex]
    float enabledF = float(light.enabled);

    float lightFactor = 0.0f;
    switch(light.type) {
        case POINT_LIGHT_TYPE:
        lightFactor = computePointLight(worldPos, normal, lightIndex);
        break;

        case DIRECTIONAL_LIGHT_TYPE:
        lightFactor = computeDirectionalLight(worldPos, normal, lightIndex);
        break;

        case SPOT_LIGHT_TYPE:
        lightFactor = computeSpotLight(worldPos, normal, lightIndex);
        break;
    }

    return enabledF * light.color * lightFactor * light.intensity;
    #undef light
}


#ifdef HARDWARE_SUPPORTS_RAY_TRACING
bool traceShadowRay(vec3 startPos, vec3 direction, float maxDistance) {
    if(!push.hasTLAS) {
        return true;
    }

    // Ray Query for shadow
    float tMin      = 0.1f;

    // Initializes a ray query object but does not start traversal
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, startPos, tMin, direction, maxDistance);

    // Start traversal: return false if traversal is complete
    while(rayQueryProceedEXT(rayQuery)) {}

    // Returns type of committed (true) intersection
    return rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT;
}

void traceRay(inout SurfaceIntersection r, vec3 startPos, vec3 direction, float maxDistance) {
    r.hasIntersection = false;
    if(!push.hasTLAS) {
        return;
    }

    // Ray Query for shadow
    float tMin      = 0.00001f;

    // Initializes a ray query object but does not start traversal
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, startPos, tMin, direction, maxDistance);

    // Start traversal: return false if traversal is complete
    while(rayQueryProceedEXT(rayQuery)) {}

    // Returns type of committed (true) intersection
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        return;
    }

    int instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
    if(instanceID < 0) {
        return;
    }
    int localGeometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
    if(localGeometryID < 0) {
        return;
    }

    #define instance (instances[nonuniformEXT(instanceID)])

    // TODO: bounds checks?
    uint geometryID = instance.firstGeometryIndex + localGeometryID;
    int triangleID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    if(triangleID < 0) {
        return;
    }
    vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);

    Geometry geometry = (geometries[nonuniformEXT(geometryID)]);

    uint index2 = nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+0)]);
    uint index0 = nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+1)]);
    uint index1 = nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+2)]);
    Vertex P2 = (geometry.vertexBuffer.v[index2]);
    Vertex P0 = (geometry.vertexBuffer.v[index0]);
    Vertex P1 = (geometry.vertexBuffer.v[index1]);

    vec2 uvPos = P0.uv * barycentrics.x + P1.uv * barycentrics.y + P2.uv * (1.0 - barycentrics.x - barycentrics.y);
    vec3 surfaceNormal = P0.normal * barycentrics.x + P1.normal * barycentrics.y + P2.normal * (1.0 - barycentrics.x - barycentrics.y);
    vec3 surfaceTangent = (P0.tangent * barycentrics.x + P1.tangent * barycentrics.y + P2.tangent * (1.0 - barycentrics.x - barycentrics.y)).xyz;
    vec3 color = P0.color * barycentrics.x + P1.color * barycentrics.y + P2.color * (1.0 - barycentrics.x - barycentrics.y);

    mat3 modelRotation = mat3(rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));
    r.hasIntersection = true;
    r.materialIndex = geometry.materialIndex;
    r.uv = uvPos;
    r.surfaceNormal = modelRotation * surfaceNormal;
    r.surfaceTangent = modelRotation * surfaceTangent;
    r.surfaceColor = color * instance.instanceColor.rgb;
    r.position = rayQueryGetIntersectionTEXT(rayQuery, true) * direction + startPos;
}

vec3 computeDirectLighting(inout RandomSampler rng, inout float lightPDF, vec3 worldPos, vec3 normal, float maxDistance) {
    #define light lights.l[i]
    vec3 lightContribution = vec3(0.0);

    if(activeLights.count <= 0) {
        lightPDF = 1.0;
        return vec3(0.0);
    }

//    float pdfInv = activeLights.count;
//    uint i = min(activeLights.count - 1, uint(floor(sampleNoise(rng) * activeLights.count)));
//    i = activeLights.indices[i];
//    lightPDF = 1.0 / pdfInv;
    lightPDF = 1.0;

    for (uint li = 0; li < activeLights.count; li++)
    {
        uint i = activeLights.indices[li];
        float enabledF = float(light.enabled);

        if(light.enabled)
        {
            vec3 L = vec3(0, 0, 1);
            switch (light.type) {
                case POINT_LIGHT_TYPE:
                L = light.position - worldPos;
                break;

                case DIRECTIONAL_LIGHT_TYPE:
                L = light.direction;
                break;

                case SPOT_LIGHT_TYPE:
                L = light.position - worldPos;
                break;
            }

            // from https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_rayquery
            float lightDistance = maxDistance;

            if(light.type != DIRECTIONAL_LIGHT_TYPE) {
                lightDistance = min(lightDistance, length(L));
            }

            float visibility = float(traceShadowRay(worldPos, normalize(L), lightDistance * enabledF));
            lightContribution += enabledF * visibility * computeLightContribution(i, worldPos, normal) /* cos term already in computeLightContribution */;
        }
    }
    return lightContribution;
    #undef light
}
#endif

#undef HARDWARE_SUPPORTS_RAY_TRACING // TODO debug only remove

vec3 calculateGI(inout RandomSampler rng, vec3 worldPos, vec3 emissive, vec3 normal, vec3 tangent, vec2 metallicRoughness, bool raytracing) {
    vec3 finalColor = vec3(0.0);

    const vec3 cameraPos = (cbo.inverseView * vec4(0, 0, 0, 1)).xyz;

    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
    #endif
        vec3 lightContribution = emissive + lights.ambientColor;
        for (uint i = 0; i < activeLights.count; i++) {
            uint lightIndex = activeLights.indices[i];
            lightContribution += computeLightContribution(lightIndex, worldPos, normal);
        }
        return lightContribution;
    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {
        vec3 lightContribution = lights.ambientColor;
        vec3 rayOrigin = worldPos;
        vec3 incomingRay = normalize(rayOrigin - cameraPos);
        const float MAX_LIGHT_DISTANCE = 5000.0f; /* TODO: specialization constant? compute properly?*/
        const uint MAX_BOUNCES = 3; /* TODO: specialization constant?*/
        float roughnessBias = 0.0f;
        float roughness = metallicRoughness.y;
        float metallic = metallicRoughness.x;

        vec3 beta = vec3(1.0f);
        int depth = 0;

        vec3 pathContribution = vec3(0.0f);
        bool lastIsSpecular = false;
        for(; depth < MAX_BOUNCES; depth++) {
            rayOrigin += normal*0.001f; // avoid self intersection

            const float oldRoughness = roughness;
            roughness = min(1, oldRoughness + roughnessBias);
            roughnessBias += oldRoughness * 0.75f; // firefly rejection

            const float u = sampleNoise(rng);

            bool sampledSpecular = u <= metallic;
            float sampledSpecularF = float(sampledSpecular);

            SurfaceIntersection intersection;
            intersection.hasIntersection = false;

            float lightPDF = 1.0f;

            vec3 lightAtPoint = computeDirectLighting(/*inout*/rng, /*inout*/lightPDF, rayOrigin, normal, MAX_LIGHT_DISTANCE);

            /*vec3 wh = normalize(lightDirection + incomingRay);
            vec3 fresnel = schlickFresnel(vec3(0.0), clamp(dot(wh, lightDirection), 0.0, 1.0)); // TODO ?*/
            vec3 fresnel = vec3(1.0);

            vec3 T = tangent;
            vec3 N = normal;
            T = normalize(T - dot(T, N) * N);
            vec3 B = cross(T, N);

            mat3 tbn = mat3(T, B, N);
            mat3 invTBN = transpose(tbn);

            float lightWeight = 1.0;

            // TODO: non-delta lights (ie area lights)
            if(false) {
                float scatteringPDF = 1.0; // TODO
                lightWeight *= powerHeuristic(1, lightPDF, 1, scatteringPDF);
            }

            lightContribution += (emissive + lightAtPoint * fresnel) * beta;

            if(depth == MAX_BOUNCES - 1)
                break;

            if(sampledSpecular) {
                // sample specular reflection
                vec3 reflectedRay = reflect(incomingRay, normal);
                traceRay(intersection, rayOrigin, reflectedRay, MAX_LIGHT_DISTANCE);
                incomingRay = reflectedRay;
            } else {
                vec3 direction;
                float pdf;
                vec3 f = sampleLambertianF(rng, roughness, invTBN * incomingRay, direction, pdf);

                if (pdf == 0.0 || dot(f, f) <= 0.0f) {
                    break;
                }

                vec3 worldSpaceDirection = tbn * normalize(-direction);
                beta *= f * abs(dot(worldSpaceDirection, N)) / pdf;

                traceRay(intersection, rayOrigin, worldSpaceDirection, MAX_LIGHT_DISTANCE);
                incomingRay = worldSpaceDirection;
            }

            if(intersection.hasIntersection) {
                #define _sample(TYPE) texture(sampler2D(textures[materials[intersection.materialIndex].TYPE], linearSampler), intersection.uv)
                beta *= _sample(albedo).rgb;
                beta *= intersection.surfaceColor;

                rayOrigin = intersection.position;
                emissive = _sample(emissive).rgb;

                vec3 T = normalize(intersection.surfaceTangent);
                vec3 N = normalize(intersection.surfaceNormal);
                vec3 B = cross(T, N);

                mat3 tbn = mat3(T, B, N);
                vec3 mappedNormal = _sample(normalMap).rgb;

                mappedNormal = mappedNormal * 2 -1;
                mappedNormal = normalize(T * mappedNormal.x + B * mappedNormal.y + N * mappedNormal.z);

                normal = mappedNormal;
                tangent = T;
                vec2 metallicRoughness = _sample(metallicRoughness).rg;
                metallic = metallicRoughness.x;
                roughness = metallicRoughness.y;
            } else {
                vec3 uv = vec3(incomingRay.x, incomingRay.z, -incomingRay.y);
                if(isnan(uv.x))
                {
                    // incomingRay-> NaN ???
                    return vec3(0, 100000, 0);
                }
                else
                {
                    vec3 skyboxColor = texture(gSkybox3D, uv).rgb;
                    lightContribution += skyboxColor * beta;
                }
                break;
            }
        }

        return lightContribution;
    }
    #endif
}

vec3 calculateReflections(inout RandomSampler rng, vec3 worldPos, vec3 emissive, vec3 normal, vec3 tangent, vec2 metallicRoughness, bool raytracing) {
    const vec3 cameraPos = (cbo.inverseView * vec4(0, 0, 0, 1)).xyz;

    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
    #endif
        if(metallicRoughness.x > 0.0f) {
            vec3 incomingRay = normalize(worldPos - cameraPos);
            vec3 reflectedRay = reflect(incomingRay, normal);
            vec3 uv = vec3(reflectedRay.x, reflectedRay.z, -reflectedRay.y);

        // not physically accurate, but looks good enough for now
            return texture(gSkybox3D, uv).rgb * metallicRoughness.x * (1-metallicRoughness.y);
        }
        return vec3(0.0);
    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {
        vec3 lightContribution = vec3(0.0);

        vec3 incomingRay = normalize(worldPos - cameraPos);
        const float MAX_LIGHT_DISTANCE = 5000.0f; /* TODO: specialization constant? compute properly?*/
        const uint MAX_BOUNCES = 3; /* TODO: specialization constant?*/
        float roughnessBias = 0.0f;
        float roughness = metallicRoughness.y;
        float metallic = metallicRoughness.x;

        vec3 beta = vec3(1.0f);
        int depth = 0;

        float lightPDF = 1.0f;
        vec3 pathContribution = vec3(0.0f);
        bool lastIsSpecular = false;
        SurfaceIntersection intersection;
        intersection.hasIntersection = false;
        for(; depth < MAX_BOUNCES; depth++) {
            const float oldRoughness = roughness;
            roughness = min(1, oldRoughness + roughnessBias);
            roughnessBias += oldRoughness * 0.75f; // firefly rejection

            const float u = sampleNoise(rng);

            // direct lighting
            float weight = 1.0f;
            bool sampledSpecular = u <= metallic;

            float sampledSpecularF = float(sampledSpecular);

            /*vec3 wh = normalize(lightDirection + incomingRay);
            vec3 fresnel = schlickFresnel(vec3(0.0), clamp(dot(wh, lightDirection), 0.0, 1.0)); // TODO ?*/
            vec3 fresnel = vec3(1.0);

            vec3 T = tangent;
            vec3 N = normal;
            T = normalize(T - dot(T, N) * N);
            vec3 B = cross(T, N);

            mat3 tbn = mat3(T, B, N);
            mat3 invTBN = transpose(tbn);

            float lightWeight = 1.0;

            // TODO: non-delta lights (ie area lights)
            if(false) {
                float scatteringPDF = 1.0; // TODO
                lightWeight *= powerHeuristic(1, lightPDF, 1, scatteringPDF);
            }


            vec3 lightAtPoint = computeDirectLighting(/*inout*/rng, /*inout*/lightPDF, worldPos, normal, MAX_LIGHT_DISTANCE);
            lightContribution += lightAtPoint * beta;

            if(depth == MAX_BOUNCES - 1)
                break;

            if(sampledSpecular) {
                // sample specular reflection
                vec3 reflectedRay = reflect(incomingRay, normal);
                traceRay(intersection, worldPos, reflectedRay, MAX_LIGHT_DISTANCE);
                incomingRay = reflectedRay;
            } else {
                vec3 direction;
                float pdf;
                vec3 f = sampleGGXF(rng, roughness, invTBN * incomingRay, direction, pdf);

                if(pdf == 0.0 || dot(f, f) <= 0.0f)
                    break;

                vec3 worldSpaceDirection = tbn * normalize(-direction);
                beta *= f * abs(dot(worldSpaceDirection, N)) / pdf;

                traceRay(intersection, worldPos, worldSpaceDirection, MAX_LIGHT_DISTANCE);
                incomingRay = worldSpaceDirection;
            }

            if(intersection.hasIntersection) {
                #define _sample(TYPE) texture(sampler2D(textures[materials[intersection.materialIndex].TYPE], linearSampler), intersection.uv)
                beta *= _sample(albedo).rgb;
                beta *= intersection.surfaceColor;

                worldPos = intersection.position;

                vec3 T = normalize(intersection.surfaceTangent);
                vec3 N = normalize(intersection.surfaceNormal);
                vec3 B = cross(T, N);

                mat3 tbn = mat3(T, B, N);
                vec3 mappedNormal = _sample(normalMap).rgb;

                mappedNormal = mappedNormal * 2 -1;
                mappedNormal = normalize(T * mappedNormal.x + B * mappedNormal.y + N * mappedNormal.z);

                normal = mappedNormal;
                tangent = T;
                vec2 metallicRoughness = _sample(metallicRoughness).rg;
                metallic = metallicRoughness.x;
                roughness = metallicRoughness.y;
            } else {
                vec3 uv = vec3(incomingRay.x, incomingRay.z, -incomingRay.y);
                if(isnan(uv.x))
                {
                    // incomingRay-> NaN ???
                    return vec3(0, 100000, 0);
                }
                else
                {
                    vec3 skyboxColor = texture(gSkybox3D, uv).rgb;
                    lightContribution += skyboxColor * beta;
                }
                break;
            }
        }

        return lightContribution;
    }
    #endif
}