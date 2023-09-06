//
// Created by jglrxavpok on 06/09/2023.
//

#pragma once

namespace Carrot::Physics {
    struct BodyUserData {
        enum class Type {
            Rigidbody,
            Character,
        };

        Type type;
        void* ptr;
    };
}