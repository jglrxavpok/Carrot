//
// Created by jglrxavpok on 02/08/2023.
//

#pragma once

#include "EditorPanel.h"
#include <engine/pathfinding/NavMeshBuilder.h>

namespace Peeler {

    class NavMeshPanel: public EditorPanel {
    public:
        explicit NavMeshPanel(Peeler::Application& app);

        virtual void draw(const Carrot::Render::Context& renderContext) override final;

    private:
        Carrot::AI::NavMeshBuilder navMeshBuilder;
        float maximumClearance = 1.8f;
        float minimumWidth = 0.5f;
        float maximumSlope = glm::radians(45.0f);
        float voxelSize = 0.5f;
    };

} // Peeler
