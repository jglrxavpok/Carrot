//
// Created by jglrxavpok on 20/12/2020.
//

#include "conversions.h"
#include <glm/gtc/type_ptr.hpp>

glm::mat4 Carrot::glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix) {
    return glm::transpose(glm::make_mat4(&assimpMatrix.a1));
}
