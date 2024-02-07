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

layout(set = 2, binding = 3, scalar) buffer ModelDataRef {
    InstanceData modelData[];
};
DEFINE_CAMERA_SET(3)

layout(location = 0) in vec2 screenUV;

vec2 project(mat4 modelview, vec3 p) {
    vec4 hPosition = cbo.jitteredProjection * modelview * vec4(p, 1.0);

    hPosition.xyz /= hPosition.w;
    vec2 projected = (hPosition.xy + 1.0) / 2.0;
    return projected;
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
    mat4 modelTransform = modelData[modelDataIndex].transform;
    mat4 modelview = cbo.view * modelTransform * clusterTransform;

    vec2 posA = project(modelview, vA.pos.xyz);
    vec2 posB = project(modelview, vB.pos.xyz);
    vec2 posC = project(modelview, vC.pos.xyz);

    vec3 barycentricsInsideTriangle = barycentrics(vec3(posA, 0), vec3(posB, 0), vec3(posC, 0), vec3(screenUV, 0));

    vec2 uv = barycentricsInsideTriangle.x * vA.uv + barycentricsInsideTriangle.y * vB.uv + barycentricsInsideTriangle.z * vC.uv;
    vec3 position = barycentricsInsideTriangle.x * vA.pos.xyz + barycentricsInsideTriangle.y * vB.pos.xyz + barycentricsInsideTriangle.z * vC.pos.xyz;

    Material material = materials[materialIndex];
    uint albedoTexture = nonuniformEXT(material.albedo);
    uint normalMap = nonuniformEXT(material.normalMap);
    uint emissiveTexture = nonuniformEXT(material.emissive);
    uint metallicRoughnessTexture = nonuniformEXT(material.metallicRoughness);
    vec4 texColor = texture(sampler2D(textures[albedoTexture], linearSampler), uv);
    texColor *= material.baseColor;
    texColor *= modelData[modelDataIndex].color;

    texColor.a = 1.0;
    if(texColor.a < 0.01) {
        discard;
    }

    GBuffer o = initGBuffer(mat4(1.0));

    o.albedo = vec4(texColor.rgb, texColor.a /* TODO * instanceColor.a*/);

    vec4 hPosition = modelview * vec4(position, 1.0);
    o.viewPosition = hPosition.xyz / hPosition.w;

    vec3 N = barycentricsInsideTriangle.x * vA.normal + barycentricsInsideTriangle.y * vB.normal + barycentricsInsideTriangle.z * vC.normal;
    vec3 T = barycentricsInsideTriangle.x * vA.tangent.xyz + barycentricsInsideTriangle.y * vB.tangent.xyz + barycentricsInsideTriangle.z * vC.tangent.xyz;

    N = mat3(modelview) * N;
    T = mat3(modelview) * T;
    float bitangentSign = barycentricsInsideTriangle.x * vA.tangent.w + barycentricsInsideTriangle.y * vB.tangent.w + barycentricsInsideTriangle.z * vC.tangent.w;

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
    o.entityID = modelData[modelDataIndex].uuid;

    vec2 metallicRoughness = texture(sampler2D(textures[metallicRoughnessTexture], linearSampler), uv).bg * material.metallicRoughnessFactor;
    o.metallicness = metallicRoughness.x;
    o.roughness = metallicRoughness.y;
    o.emissiveColor = texture(sampler2D(textures[emissiveTexture], linearSampler), uv).rgb * material.emissiveColor;

    mat4 previousFrameModelTransform = modelData[modelDataIndex].lastFrameTransform;
    mat4 previousFrameModelview = previousFrameCBO.view * previousFrameModelTransform * clusterTransform;
    vec4 previousFrameClipPos = previousFrameModelview * vec4(position, 1.0);
    vec3 previousFrameViewPosition = previousFrameClipPos.xyz / previousFrameClipPos.w;
    vec4 clipPos = cbo.nonJitteredProjection * vec4(o.viewPosition, 1.0);
    vec4 previousClipPos = previousFrameCBO.nonJitteredProjection * vec4(previousFrameViewPosition, 1.0);
    o.motionVector = previousClipPos.xyz/previousClipPos.w - clipPos.xyz/clipPos.w;


    outputGBuffer(o, mat4(1.0));
}