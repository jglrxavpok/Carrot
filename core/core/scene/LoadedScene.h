//
// Created by jglrxavpok on 27/10/2022.
//

#pragma once

#include <memory>
#include <unordered_map>
#include "core/io/vfs/VirtualFileSystem.h"
#include <core/render/Meshlet.h>
#include <core/render/VertexTypes.h>
#include <core/render/Skeleton.h>
#include <core/render/Animation.h>
#include <core/containers/Vector.hpp>
#include <core/containers/Pair.hpp>
#include <core/render/VkAccelerationStructureHeader.h>
#include <glm/glm.hpp>

namespace Carrot {
    struct Animation;
}

namespace Carrot::Render {
    struct LoadedMaterial {
        enum class BlendMode {
            None,
            Blend,
        };

        std::string name;

        BlendMode blendMode = BlendMode::None;

        /**
         * Path to albedo texture, empty if none
         */
        Carrot::IO::VFS::Path albedo;
        glm::vec4 baseColorFactor{1.0f};

        /**
         * Path to normalMap texture, empty if none (Blue = metalness, Green = roughness)
         */
        Carrot::IO::VFS::Path normalMap;

        /**
         * Path to metallic+roughness texture, empty if none
         */
        Carrot::IO::VFS::Path metallicRoughness;

        /**
         * Metallic factor, multiplied with metallic texture
         */
        float metallicFactor = 1.0f;

        /**
         * Roughness factor, multiplied with roughness texture
         */
        float roughnessFactor = 1.0f;

        /**
         * Path to occlusion texture, empty if none (Red = occlusion factor)
         */
        Carrot::IO::VFS::Path occlusion;

        /**
         * Path to occlusion texture, empty if none
         */
        Carrot::IO::VFS::Path emissive;

        /**
         * Emissive factor, multiplied with emissive texture
         */
        glm::vec3 emissiveFactor{1.0f};
    };

    struct LoadedPrimitive {
        /**
         * Debug name
         */
        std::string name;

        /**
         * Does this primitive use a skin?
         */
        bool isSkinned = false;

        bool hadTangents = false;
        bool hadNormals = false;
        bool hadTexCoords = false;

        /**
         * Transform of this primitive, relative to root of scene.
         * Already contains the transform of parent nodes
         */
        glm::mat4 transform{1.0f};

        /**
         * Buffer containing vertices of the primitive
         */
        std::vector<Carrot::Vertex> vertices;
        std::vector<Carrot::SkinnedVertex> skinnedVertices;

        glm::vec3 minPos{0.0f};
        glm::vec3 maxPos{0.0f};

        /**
         * Buffer containing indices of the primitive
         */
        std::vector<std::uint32_t> indices;

        /**
         * Index of material to use inside the 'materials' member of LoadedScene
         */
        std::int64_t materialIndex = -1;

        std::vector<std::uint32_t> meshletVertexIndices; // all vertices of all meshlets (indices of vertices inside original vertex buffer, ie 'vertices' in this struct)
        std::vector<std::uint32_t> meshletIndices; // all triangles of all meshlets (indices of vertices inside meshletVertexIndices)
        std::vector<Meshlet> meshlets;
    };

    /**
     * Represents an already built BLAS for a given mesh + transform combo
     */
    struct PrecomputedBLAS {
        /// Actual bytes of the BLAS
        Carrot::Vector<std::uint8_t> blasBytes;

        /// Compatibility information and sizes
        const Carrot::VkAccelerationStructureHeader& getHeader() const {
           return *reinterpret_cast<const Carrot::VkAccelerationStructureHeader*>(blasBytes.data());
        }
    };

    /**
     * Represents a scene loaded from a model file. Abstracted from the model file,
     *  but should be close to Carrot-ready format
     */
    struct LoadedScene {
        std::string debugName;

        std::vector<LoadedMaterial> materials;
        std::vector<LoadedPrimitive> primitives;
        std::unique_ptr<Carrot::Render::Skeleton> nodeHierarchy;

        // [primitiveIndex][groupIndex]
        using PrecomputedBLASes = std::unordered_map<Carrot::Pair<std::uint32_t, std::uint32_t>, PrecomputedBLAS>;

        std::unordered_map<NodeKey, PrecomputedBLASes> precomputedBLASes;

        // [primitiveIndex][bone name] -> boneIndex
        std::unordered_map<int, std::unordered_map<std::string, std::uint32_t>> boneMapping;
        std::unordered_map<int, std::unordered_map<std::string, glm::mat4>> offsetMatrices;

        /// id -> index in animationData
        std::map<std::string, std::uint32_t> animationMapping{};
        std::vector<Carrot::Animation> animationData{};
    };
}