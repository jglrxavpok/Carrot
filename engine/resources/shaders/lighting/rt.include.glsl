
struct SurfaceIntersection {
    vec3 position;
    uint materialIndex;
    vec3 surfaceNormal;
    vec3 surfaceTangent;
    vec3 surfaceColor;
    vec2 uv;
    float bitangentSign;
    bool hasIntersection;
};

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
SurfaceIntersection intersection;

void traceRayWithSurfaceInfo(inout SurfaceIntersection r, vec3 startPos, vec3 direction, float maxDistance) {
    r.hasIntersection = false;
    if(!push.hasTLAS) {
        return;
    }

    float tMin      = 0.00001f;

    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT, 0xFF, startPos, tMin, direction, maxDistance);

    while(rayQueryProceedEXT(rayQuery)){}

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

    Instance instance = instances[nonuniformEXT(instanceID)];
    r.surfaceColor = instance.instanceColor.rgb;

    // TODO: bounds checks?
    uint geometryID = instance.firstGeometryIndex + localGeometryID;
    int triangleID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    if(triangleID < 0) {
        return;
    }
    vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);


    Geometry geometry = (geometries[nonuniformEXT(geometryID)]);

    vec3 surfaceNormal;
    vec3 surfaceTangent;
    vec3 color;
    const uint geometryFormat = geometry.geometryFormat;
    switch(geometryFormat) {
        case GEOMETRY_FORMAT_DEFAULT: {
            #define index2 (nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+0)]))
            #define index0 (nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+1)]))
            #define index1 (nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+2)]))

            #define P2 (geometry.vertexBuffer.v[index2])
            #define P0 (geometry.vertexBuffer.v[index0])
            #define P1 (geometry.vertexBuffer.v[index1])

            r.uv = P0.uv * barycentrics.x + P1.uv * barycentrics.y + P2.uv * (1.0 - barycentrics.x - barycentrics.y);
            surfaceNormal = normalize(P0.normal * barycentrics.x + P1.normal * barycentrics.y + P2.normal * (1.0 - barycentrics.x - barycentrics.y));

            vec4 tangentWithBitangentSign = (P0.tangent * barycentrics.x + P1.tangent * barycentrics.y + P2.tangent * (1.0 - barycentrics.x - barycentrics.y));
            surfaceTangent = normalize(tangentWithBitangentSign.xyz);
            r.bitangentSign = tangentWithBitangentSign.w;
            r.surfaceColor *= P0.color * barycentrics.x + P1.color * barycentrics.y + P2.color * (1.0 - barycentrics.x - barycentrics.y);
            #undef P0
            #undef P1
            #undef P2

            #undef index2
            #undef index0
            #undef index1
        } break;

        case GEOMETRY_FORMAT_PACKED: {
            IndexBuffer16 idxBuffer = IndexBuffer16(uint64_t(geometry.indexBuffer));
            #define index2 (nonuniformEXT(idxBuffer.i[nonuniformEXT(triangleID*3+0)]))
            #define index0 (nonuniformEXT(idxBuffer.i[nonuniformEXT(triangleID*3+0)]))
            #define index1 (nonuniformEXT(idxBuffer.i[nonuniformEXT(triangleID*3+2)]))

            PackedVertexBuffer vtxBuffer = PackedVertexBuffer(uint64_t(geometry.vertexBuffer));
            #define P2 (vtxBuffer.v[index2])
            #define P0 (vtxBuffer.v[index0])
            #define P1 (vtxBuffer.v[index1])

            r.uv = P0.uv * barycentrics.x + P1.uv * barycentrics.y + P2.uv * (1.0 - barycentrics.x - barycentrics.y);
            surfaceNormal = normalize(P0.normal * barycentrics.x + P1.normal * barycentrics.y + P2.normal * (1.0 - barycentrics.x - barycentrics.y));
            vec4 tangentWithBitangentSign = (P0.tangent * barycentrics.x + P1.tangent * barycentrics.y + P2.tangent * (1.0 - barycentrics.x - barycentrics.y));
            surfaceTangent = normalize(tangentWithBitangentSign.xyz);
            r.bitangentSign = tangentWithBitangentSign.w;
            r.surfaceColor *= P0.color * barycentrics.x + P1.color * barycentrics.y + P2.color * (1.0 - barycentrics.x - barycentrics.y);
            #undef P0
            #undef P1
            #undef P2

            #undef index2
            #undef index0
            #undef index1
        } break;

        default: {
            // maybe crashing would be better? not sure
            r.hasIntersection = false;
            return;
        } break;
    }

    mat3 normalMatrix = mat3(rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));
    normalMatrix = inverse(normalMatrix);
    normalMatrix = transpose(normalMatrix);
    r.hasIntersection = true;
    r.materialIndex = geometry.materialIndex;
    r.surfaceNormal = normalize(normalMatrix * surfaceNormal);
    r.surfaceTangent = normalize(normalMatrix * surfaceTangent);
    r.position = rayQueryGetIntersectionTEXT(rayQuery, true) * direction + startPos;
}
#else
void traceRayWithSurfaceInfo(inout SurfaceIntersection r, vec3 startPos, vec3 direction, float maxDistance) {
    r.hasIntersection = false;
}
#endif