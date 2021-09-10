//
// Created by jglrxavpok on 10/09/2021.
//

#pragma once

#include <functional>

namespace Carrot {
    class Timer {
    public:
        explicit Timer(double period);

        void tick(double dt);

        explicit operator bool() const;

        void setAction(const std::function<void()>& action);

    private:
        double time = 0.0;
        double period = 0.0;
        std::function<void()> action;
    };
}
