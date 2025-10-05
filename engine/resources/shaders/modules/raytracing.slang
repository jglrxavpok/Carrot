module raytracing;

import modules.base;
import modules.raytracing_capabilities;
import modules.vertex_buffers;
import modules.materials;

namespace Raytracing {
    typealias Index = uint32_t;
    typealias Index16 = uint16_t;

    public enum BLASGeometryFormat: uint8_t {
        Default = 0, // Carrot::Vertex
        ClusterCompressed = 1, // Carrot::PackedVertex
    };

    struct Geometry {
        uint64_t vertexBufferAddress;
        uint64_t indexBufferAddress;
        uint64_t transformAddress;
        uint32_t materialIndex;
        BLASGeometryFormat format;

        internal SurfaceIntersection computeSpecificSurfaceInfo<VertexType>(
            in MaterialSystem materialSystem, 
            in Instance instance, 
            in Geometry geometry, 
            uint3 indices,
            float2 barycentrics, 
            in float3x3 objectRotation)
            where VertexType: IVertex
        {
            SurfaceIntersection result;
            VertexType* pVertices = Ptr<VertexType>(vertexBufferAddress);
            const uint index2 = indices[0];
            const uint index0 = indices[1];
            const uint index1 = indices[2];

            const VertexType vertex0 = pVertices[index0];
            const VertexType vertex1 = pVertices[index1];
            const VertexType vertex2 = pVertices[index2];

            const float baryZ = 1 - barycentrics.x - barycentrics.y;
            const float2 uv = vertex0.getUV() * barycentrics.x + vertex1.getUV() * barycentrics.y + vertex2.getUV() * baryZ;
            const float3 surfaceNormal = normalize(vertex0.getNormal() * barycentrics.x + vertex1.getNormal() * barycentrics.y + vertex2.getNormal() * baryZ);
            const float4 surfaceTangentWithBitangentSign = vertex0.getTangent() * barycentrics.x + vertex1.getTangent() * barycentrics.y + vertex2.getTangent() * baryZ;
            const float3 surfaceTangent = normalize(surfaceTangentWithBitangentSign.xyz);
            const float3 surfaceColor = vertex0.getColor() * barycentrics.x + vertex1.getColor() * barycentrics.y + vertex2.getColor() * baryZ;
            
            float3x4 geometryTransform = (Ptr<float3x4>(transformAddress))[0];
            const float3x3 normalMatrix = transpose(inverse(objectRotation * float3x3(geometryTransform)));
            result.worldNormal = normalize(normalMatrix * surfaceNormal);
            result.worldTangent = normalize(normalMatrix * surfaceTangent);

            result.computedMaterial = materialSystem.materials[geometry.materialIndex].Compute(materialSystem, materialSystem.linearSampler, uv);
            result.computedMaterial.albedo *= instance.instanceColor;

            // TODO: normal mapping
//            result.computedMaterial.albedo = float4(result.worldNormal, 1);

            return result;
        }

        Optional<SurfaceIntersection> computeSurfaceInfo(in MaterialSystem materialSystem, in Instance instance, in Geometry geometry, uint triangleID, float2 barycentrics, in float3x3 objectRotation) {
            uint3 indices;
            switch(format) {
                case BLASGeometryFormat::Default: {
                    Index* pIndices = Ptr<Index>(indexBufferAddress);
                    indices.x = pIndices[triangleID * 3 + 0];
                    indices.y = pIndices[triangleID * 3 + 1];
                    indices.z = pIndices[triangleID * 3 + 2];
                    return computeSpecificSurfaceInfo<Vertex>(materialSystem, instance, geometry, indices, barycentrics, objectRotation);
                }

                case BLASGeometryFormat::ClusterCompressed: {
                    Index16* pIndices = Ptr<Index16>(indexBufferAddress);
                    indices.x = pIndices[triangleID * 3 + 0];
                    indices.y = pIndices[triangleID * 3 + 1];
                    indices.z = pIndices[triangleID * 3 + 2];
                    return computeSpecificSurfaceInfo<PackedVertex>(materialSystem, instance, geometry, indices, barycentrics, objectRotation);
                }
            }
            return none;
        }
    }

    struct Instance {
        float4 instanceColor;
        uint firstGeometryIndex;
    }

    public struct Data {
        public RaytracingAccelerationStructure tlas;
        public Texture2D noiseTexture;
        StructuredBuffer<Geometry, ScalarDataLayout> geometries;
        StructuredBuffer<Instance, ScalarDataLayout> instances;
    }

    public struct HardwareCapability: IRaytracingCapabilities {
        private Data data;

        public __init(Data dataIn) {
            data = dataIn;
        }

        public uint getLightSampleCount() {
            // TODO: more than one sample if light importance sampling
            return 1;
        }

        public uint getAOSampleCount() {
            return 4;
        }

        public bool checkVisibility(float3 from, float3 to) {
            // TODO: attempt screen space first?

            RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> query;
            RayDesc ray;
            float3 deltaPos = to-from;
            float distance = length(deltaPos);
            ray.Direction = deltaPos / distance;
            ray.Origin = from;
            ray.TMin = 0.0f;
            ray.TMax = distance;
            query.TraceRayInline(data.tlas, RAY_FLAG_NONE, 0xFFFFu, ray);
            query.Proceed();

            return query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
        }

        public Optional<SurfaceIntersection> raytrace(in MaterialSystem materials, Optional<float2> screenSpaceUV, float3 from, float3 direction, float maxDistance) {
            RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_CULL_NON_OPAQUE> query;
            RayDesc ray;
            ray.Direction = direction;
            ray.Origin = from + direction * 0.01;
            ray.TMin = 0.0f;
            ray.TMax = maxDistance;
            query.TraceRayInline(data.tlas, RAY_FLAG_NONE, 0xFFFFu, ray);
            while(query.Proceed());

            if(query.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
                return none; // nothing touched
            }

            int instanceID = query.CommittedRayInstanceId();
            if(instanceID < 0) {
                return none;
            }

            int localGeometryID = query.CommittedRayGeometryIndex();
            if(localGeometryID < 0) {
                return none;
            }

            const Instance instance = data.instances[instanceID];
            uint globalGeometryID = instance.firstGeometryIndex + localGeometryID;
            int triangleID = query.CommittedRayPrimitiveIndex();
            if(triangleID < 0) {
                return none;
            }

            const float2 barycentrics = query.CommittedRayBarycentrics();
            const Geometry geometry = data.geometries[globalGeometryID];
            const float3x3 objectMatrix = transpose(float3x3(query.CommittedObjectToWorld4x3()));

            Optional<SurfaceIntersection> result = geometry.computeSurfaceInfo(materials, instance, geometry, triangleID, barycentrics, objectMatrix);
            if(let intersection = result) {
                SurfaceIntersection copy = intersection;
                copy.worldPosition = query.CommittedRayT() * direction + from;
                return copy;
            } else {
                return none;
            }
        }
    }
}
