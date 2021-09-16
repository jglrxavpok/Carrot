#include <iostream>
#include <GLFW/glfw3.h>
#include "engine/Engine.h"
#include "engine/constants.h"
#include "stb_image.h"
#include "Game.h"
#include "engine/Configuration.h"

#ifdef TRACY_ENABLE
void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    TracyAllocS(ptr, count, 20);
    return ptr;
}

void operator delete(void* ptr) noexcept{
    TracyFreeS(ptr, 20);
    free(ptr);
}
#endif

int main() {
    std::ios::sync_with_stdio(false);
    // try {
    glfwInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    NakedPtr<GLFWwindow> window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);


    int w, h, n;
    auto* icon32Pixels = stbi_load("resources/icon32.png", &w, &h, &n, 4);
    auto* icon64Pixels = stbi_load("resources/icon64.png", &w, &h, &n, 4);
    auto* icon128Pixels = stbi_load("resources/icon128.png", &w, &h, &n, 4);
    GLFWimage icons[] = {
            {
                    .width = 32,
                    .height = 32,
                    .pixels = icon32Pixels,
            },
            {
                    .width = 64,
                    .height = 64,
                    .pixels = icon64Pixels,
            },
            {
                    .width = 128,
                    .height = 128,
                    .pixels = icon128Pixels,
            },
    };
    glfwSetWindowIcon(window.get(), 3, icons);
    stbi_image_free(icon32Pixels);
    stbi_image_free(icon64Pixels);
    stbi_image_free(icon128Pixels);

    // create new scope, as the destructor requires the window to *not* be terminated at its end
    // otherwise this creates a DEP exception when destroying the surface provided by GLFW
    {
        Carrot::Configuration config;
#ifdef ENABLE_VR
        config.runInVR = true;
#endif
        Carrot::Engine engine{window, config};
        engine.run();
    }

    glfwDestroyWindow(window.get());
    glfwTerminate();
    /* } catch (const std::exception& e) {
         std::cerr << "Fatal exception happened: " << e.what() << std::endl;
         throw e;
     }*/
    return 0;
}

void Carrot::Engine::initGame() {
    game = std::make_unique<Game::Game>(*this);
}
