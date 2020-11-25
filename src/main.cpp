#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "CarrotEngine.h"

int main() {
    CarrotEngine engine{};
    try {
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception happened: " << e.what() << std::endl;
    }
    return 0;
}
