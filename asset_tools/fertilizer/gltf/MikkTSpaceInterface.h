//
// Created by jglrxavpok on 19/12/2022.
//

#pragma once

#include <mikktspace.h>
#include <core/scene/LoadedScene.h>

namespace Fertilizer {
    struct ExpandedVertex {
        std::uint32_t originalIndex = 0;

        std::optional<std::uint32_t> newIndex;
        Carrot::SkinnedVertex vertex{};
    };

    struct ExpandedMesh {
        std::vector<ExpandedVertex> vertices;

        std::vector<std::vector<std::uint32_t>> duplicatedVertices; // per vertex in the original vertex buffer, where are the copies inside the expanded 'vertices' array
    };

    bool generateTangents(ExpandedMesh& mesh);
}