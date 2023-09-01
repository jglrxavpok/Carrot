//
// Created by jglrxavpok on 01/09/2023.
//

#include "../engine/test_game_main.cpp"
#include <core/io/Logging.hpp>

using namespace Carrot;

int main() {
    Carrot::Log::info("Test");
    Carrot::Log::info("Test %d", 42);

    Carrot::Log::debug("other test");
    Carrot::Log::debug("other test %d", 1);
    Carrot::Log::warn("other test %d", 2);
    Carrot::Log::warn("other test %s", "hiii");
    Carrot::Log::error("other test %s %s", "hiii", "hiii2");
    Carrot::Log::error("other test %s %s %s", "hiii", "hiii2", "hiii3");
    Carrot::Log::error("other test %s %s %s %s", "hiii", "hiii2", "hiii3", "hiii4");

    return 0;
}