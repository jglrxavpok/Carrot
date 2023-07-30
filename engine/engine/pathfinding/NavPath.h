//
// Created by jglrxavpok on 29/07/2023.
//

#pragma once

namespace Carrot::AI {

    /// Represents a Path, result from pathfinding
    struct NavPath {
        std::vector<glm::vec3> waypoints;
    };

} // Carrot::AI
