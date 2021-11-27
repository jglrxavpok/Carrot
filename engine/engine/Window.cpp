//
// Created by jglrxavpok on 17/09/2021.
//

#include "Window.h"
#include "core/exceptions/Exceptions.h"
#include <list>
#include <vector>
#include <stb_image.h>

namespace Carrot {
    Window::Window(std::uint32_t width, std::uint32_t height, Configuration config) {
        if(!glfwInit()) {
            throw Carrot::Exceptions::IrrecoverableError("Could not init GLFW");
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindow = glfwCreateWindow(width, height, config.applicationName.c_str(), nullptr, nullptr);

        if(config.icon16.has_value()) {
            set16x16Icon(*config.icon16);
        }
        if(config.icon32.has_value()) {
            set32x32Icon(*config.icon32);
        }
        if(config.icon64.has_value()) {
            set64x64Icon(*config.icon64);
        }
        if(config.icon128.has_value()) {
            set128x128Icon(*config.icon128);
        }
    }

    Window::~Window() {
        if(!glfwWindow) {
            return;
        }

        glfwDestroyWindow(glfwWindow);
        glfwWindow = nullptr;
        glfwTerminate();
    }

    void Window::updateIcons() {
        std::vector<std::uint8_t*> iconPixels{icons.size(), nullptr};
        std::vector<GLFWimage> iconImages{icons.size()};

        std::uint32_t i = 0;
        for (const auto& icon : icons) {
            int w, h, n;
            auto buffer = icon.image.readAll();
            iconPixels[i] = stbi_load_from_memory(buffer.get(), icon.image.getSize(), &w, &h, &n, 4);
            assert(w == icon.width);
            assert(h == icon.height);
            iconImages[i] = GLFWimage {
                    .width = static_cast<int>(icon.width),
                    .height = static_cast<int>(icon.height),
                    .pixels = iconPixels[i],
            };

            i++;
        }

        glfwSetWindowIcon(glfwWindow, static_cast<int>(iconImages.size()), iconImages.data());

        for(auto* ptr : iconPixels) {
            stbi_image_free(ptr);
        }
    }

    void Window::setIcon(std::uint32_t size, const Carrot::IO::Resource& resource) {
        auto memcopy = resource.copyToMemory();
        icons.emplace_back(std::move(Icon {
            .width = size,
            .height = size,
            .image = std::move(memcopy)
        }));

        updateIcons();
    }

    void Window::set16x16Icon(const Carrot::IO::Resource& icon) {
        setIcon(16, icon);
    }

    void Window::set32x32Icon(const Carrot::IO::Resource& icon) {
        setIcon(32, icon);
    }

    void Window::set64x64Icon(const Carrot::IO::Resource& icon) {
        setIcon(64, icon);
    }

    void Window::set128x128Icon(const Carrot::IO::Resource& icon) {
        setIcon(128, icon);
    }

    void Window::setTitle(std::string_view title) {
        glfwSetWindowTitle(glfwWindow, title.data());
    }

    void Window::getFramebufferSize(std::int32_t& w, std::int32_t& h) const {
        int width, height;
        glfwGetFramebufferSize(glfwWindow, &width, &height);
        w = width;
        h = height;
    }

    void Window::maximise() {
        glfwMaximizeWindow(glfwWindow);
    }

    bool Window::isMaximised() const {
        return glfwGetWindowAttrib(glfwWindow, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    void Window::getWindowSize(std::int32_t& w, std::int32_t& h) const {
        int width, height;
        glfwGetWindowSize(glfwWindow, &width, &height);
        w = width;
        h = height;
    }

    void Window::getPosition(std::int32_t& x, std::int32_t& y) const {
        int xPos, yPos;
        glfwGetWindowPos(glfwWindow, &xPos, &yPos);
        x = xPos;
        y = yPos;
    }

    void Window::setPosition(std::int32_t x, std::int32_t y) {
        glfwSetWindowPos(glfwWindow, x, y);
    }

    void Window::setWindowSize(std::int32_t w, std::int32_t h) {
        glfwSetWindowSize(glfwWindow, w, h);
    }
}