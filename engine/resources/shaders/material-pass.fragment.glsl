#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_image_int64 : require

#include <includes/buffers.glsl>
#include <includes/camera.glsl>
#include <includes/clusters.glsl>
#include <includes/math.glsl>
#include <includes/gbuffer.glsl>
#include "includes/gbuffer_input.glsl"
#include "includes/gbuffer_output.glsl"
#include "includes/materials.glsl"

DEFINE_GBUFFER_INPUTS(0)
MATERIAL_SYSTEM_SET(1)
layout(r64ui, set = 2, binding = 0) uniform u64image2D visibilityBuffer;
layout(set = 2, binding = 1, scalar) buffer ClusterRef {
    Cluster clusters[];
};

layout(set = 2, binding = 2, scalar) buffer ClusterInstanceRef {
    ClusterInstance instances[];
};

layout(set = 2, binding = 3, std430) buffer ModelDataRef {
    ClusterBasedModelData modelData[];
};
DEFINE_CAMERA_SET(3)

layout(location = 0) in vec2 screenUV;

// https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf
// Deferred Attribute Interpolation for Memory-Efficient Deferred Shading
// Scheid, Dachsbacher
struct Interpolator {
    vec3 invWi;
    vec3 barycentrics;
    float invDivider; // 1 / (wi * barycentrics)
};

vec2 toScreenUV(vec4 homogeneousVec) {
    vec2 projected = (homogeneousVec.xy / homogeneousVec.w + 1.0) / 2.0;
    return projected;
}

vec3 interpolatorBarycentrics(vec4 aClipSpace, vec4 bClipSpace, vec4 cClipSpace) {
    const vec2 ndcUV = screenUV * 2 - 1;

    const float D = determinant(mat2(cClipSpace.xy - bClipSpace.xy, aClipSpace.xy - bClipSpace.xy));
    float lambda1 = (bClipSpace.y - cClipSpace.y) * (ndcUV.x - cClipSpace.x) + (cClipSpace.x - bClipSpace.x) * (ndcUV.y - cClipSpace.y);
    float lambda2 = (cClipSpace.y - aClipSpace.y) * (ndcUV.x - cClipSpace.x) + (aClipSpace.x - cClipSpace.x) * (ndcUV.y - cClipSpace.y);
    lambda1 /= D;
    lambda2 /= D;
    return vec3(lambda1, lambda2, 1 - lambda1 - lambda2);
}

Interpolator createInterpolator(in Vertex vA, in Vertex vB, in Vertex vC, in mat4 modelview, float ndcDepth) {
    vec4 pointViewSpace = (cbo.inverseNonJitteredProjection * vec4(vec3(screenUV * 2.0 - 1.0, ndcDepth * 2 - 1), 1.0));
    pointViewSpace.xyz *= pointViewSpace.w;

    const mat4 clipSpaceFromMesh = cbo.jitteredProjection * modelview;
    vec4 aClipSpace = clipSpaceFromMesh * vA.pos;
    vec4 bClipSpace = clipSpaceFromMesh * vB.pos;
    vec4 cClipSpace = clipSpaceFromMesh * vC.pos;

    Interpolator interpolator;
    interpolator.invWi = 1.0f / vec3(aClipSpace.w, bClipSpace.w, cClipSpace.w);
    aClipSpace.xy /= aClipSpace.w;
    bClipSpace.xy /= bClipSpace.w;
    cClipSpace.xy /= cClipSpace.w;

    interpolator.barycentrics = interpolatorBarycentrics(aClipSpace, bClipSpace, cClipSpace);
    interpolator.invDivider = 1.0f / dot(interpolator.invWi, interpolator.barycentrics);
    return interpolator;
}

float interpolate1D(in Interpolator interlopator, float a, float b, float c) {
    float numerator =
        interlopator.barycentrics.x * a * interlopator.invWi.x
    +   interlopator.barycentrics.y * b * interlopator.invWi.y
    +   interlopator.barycentrics.z * c * interlopator.invWi.z;

    return numerator * interlopator.invDivider;
}

vec2 interpolate2D(in Interpolator interlopator, vec2 a, vec2 b, vec2 c) {
    vec2 numerator =
        interlopator.barycentrics.x * a * interlopator.invWi.x
    +   interlopator.barycentrics.y * b * interlopator.invWi.y
    +   interlopator.barycentrics.z * c * interlopator.invWi.z;

    return numerator * interlopator.invDivider;
}

vec3 interpolate3D(in Interpolator interlopator, vec3 a, vec3 b, vec3 c) {
    vec3 numerator =
        interlopator.barycentrics.x * a * interlopator.invWi.x
    +   interlopator.barycentrics.y * b * interlopator.invWi.y
    +   interlopator.barycentrics.z * c * interlopator.invWi.z;

    return numerator * interlopator.invDivider;
}

void main() {
    uvec2 visibilityBufferImageSize = imageSize(visibilityBuffer);
    ivec2 pixelCoords = ivec2(visibilityBufferImageSize * screenUV);
    uint64_t visibilityBufferSample = imageLoad(visibilityBuffer, pixelCoords).r;
    if(visibilityBufferSample == 0) {
        discard;
    }

    double visibilityBufferDepth = uint(0xFFFFFFFFu - (visibilityBufferSample >> 32u)) / double(0xFFFFFFFFu);
    gl_FragDepth = float(visibilityBufferDepth);

    uint low = uint(visibilityBufferSample);

    uint triangleIndex = low & 0x7Fu;
    uint instanceIndex = (low >> 7) & 0x1FFFFFFu;
    uint clusterID = instances[instanceIndex].clusterID;
    uint materialIndex = instances[instanceIndex].materialIndex;

#define getVertex(n) (clusters[clusterID].vertices.v[clusters[clusterID].indices.i[(n)]])
    Vertex vA = getVertex(triangleIndex * 3 + 0);
    Vertex vB = getVertex(triangleIndex * 3 + 1);
    Vertex vC = getVertex(triangleIndex * 3 + 2);

    mat4 clusterTransform = clusters[clusterID].transform;
    uint modelDataIndex = instances[instanceIndex].instanceDataIndex;
    mat4 modelTransform = modelData[modelDataIndex].instanceData.transform;
    mat4 modelview = cbo.view * modelTransform * clusterTransform;

    Interpolator interlopator = createInterpolator(vA, vB, vC, modelview, float(visibilityBufferDepth));

    vec2 uv = interpolate2D(interlopator, vA.uv, vB.uv, vC.uv);
    vec3 position = interpolate3D(interlopator, vA.pos.xyz, vB.pos.xyz, vC.pos.xyz);

    Material material = materials[materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    uint normalMap = nonuniformEXT(material.normalMap);
    uint emissiveTexture = nonuniformEXT(material.emissive);
    uint metallicRoughnessTexture = nonuniformEXT(material.metallicRoughness);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    texColor *= material.baseColor;
    texColor *= modelData[modelDataIndex].instanceData.color;

    texColor.a = 1.0;
    if(texColor.a < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer(mat4(1.0));

    o.albedo = vec4(texColor.rgb, texColor.a /* TODO * instanceColor.a*/);

    vec4 hPosition = modelview * vec4(position, 1.0);
    o.viewPosition = hPosition.xyz / hPosition.w;

    vec3 N = interpolate3D(interlopator, vA.normal, vB.normal, vC.normal);
    vec3 T = interpolate3D(interlopator, vA.tangent.xyz, vB.tangent.xyz, vC.tangent.xyz);

    N = mat3(modelview) * N;
    T = mat3(modelview) * T;
    float bitangentSign = interpolate1D(interlopator, vA.tangent.w, vB.tangent.w, vC.tangent.w);

    vec3 N_ = normalize(N);
    vec3 T_ = normalize(T - dot(T, N_) * N_);

    vec3 B_ = normalize(bitangentSign * cross(N_, T_));

    vec3 mappedNormal = texture(sampler2D(textures[normalMap], linearSampler), uv).xyz;
    mappedNormal = mappedNormal * 2 -1;
    mappedNormal = normalize(mappedNormal.x * T_ + mappedNormal.y * B_ + mappedNormal.z * N_);

    N_ = mappedNormal;
    T_ = normalize(T - dot(T, N_) * N_);
    B_ = normalize(bitangentSign * cross(N_, T_));

    o.viewTBN = mat3(T_, B_, N_);

    o.intProperty = IntPropertiesRayTracedLighting;
    o.entityID = modelData[modelDataIndex].instanceData.uuid;

    vec2 metallicRoughness = texture(sampler2D(textures[metallicRoughnessTexture], linearSampler), uv).bg * material.metallicRoughnessFactor;
    o.metallicness = metallicRoughness.x;
    o.roughness = metallicRoughness.y;
    o.emissiveColor = texture(sampler2D(textures[emissiveTexture], linearSampler), uv).rgb * material.emissiveColor;

    mat4 previousFrameModelTransform = modelData[modelDataIndex].instanceData.lastFrameTransform;
    mat4 previousFrameModelview = previousFrameCBO.view * previousFrameModelTransform * clusterTransform;
    vec4 previousFrameClipPos = previousFrameModelview * vec4(position, 1.0);
    vec3 previousFrameViewPosition = previousFrameClipPos.xyz / previousFrameClipPos.w;
    vec4 clipPos = cbo.nonJitteredProjection * vec4(o.viewPosition, 1.0);
    vec4 previousClipPos = previousFrameCBO.nonJitteredProjection * vec4(previousFrameViewPosition, 1.0);
    o.motionVector = previousClipPos.xyz/previousClipPos.w - clipPos.xyz/clipPos.w;


    outputGBuffer(o, mat4(1.0));
}