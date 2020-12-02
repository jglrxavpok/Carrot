#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "Engine.h"
#include "constants.h"

int main() {
   // try {
        glfwInit();

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        NakedPtr<GLFWwindow> window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);

        // create new scope, as the destructor requires the window to *not* be terminated at its end
        // otherwise this creates a DEP exception when destroying the surface provided by GLFW
        {
            Carrot::Engine engine{window};
            engine.run();
        }

        glfwDestroyWindow(window.get());
        glfwTerminate();
    /*} catch (const std::exception& e) {
        std::cerr << "Fatal exception happened: " << e.what() << std::endl;
        throw e;
    }*/
    return 0;
}
