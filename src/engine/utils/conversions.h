#pragma once

#include <glm/matrix.hpp>
#include <assimp/types.h>

namespace Carrot {
    glm::mat4 glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix);
}