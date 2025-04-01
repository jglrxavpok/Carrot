//
// Created by jglrxavpok on 06/09/2023.
//

#pragma once

namespace Carrot::Physics {
    class BaseBody;

    struct BodyUserData {
        enum class Type {
            Rigidbody,
            Character,
        };

        Type type;
        BaseBody* ptr;
    };
}