//
// Created by jglrxavpok on 10/09/2021.
//

#include "Timer.h"

namespace Carrot {
    Timer::Timer(double period): period(period), time(period) {}

    Timer::operator bool() const {
        return time < 0.0;
    }

    void Timer::tick(double dt) {
        if(time < 0.0) {
            if(action) {
                action();
            }
            time = period;
        } else {
            time -= dt;
        }
    }

    void Timer::setAction(const std::function<void()>& action) {
        this->action = action;
    }
}