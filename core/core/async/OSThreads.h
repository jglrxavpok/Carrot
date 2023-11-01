//
// Created by jglrxavpok on 04/02/2022.
//

#pragma once

#include <string>
#include <thread>

namespace Carrot::Threads {
    void setName(std::jthread& thread, std::string_view name);
    void setName(std::thread& thread, std::string_view name);
    void setName(void* nativeHandle, std::string_view name);

    void reduceCPULoad();
}