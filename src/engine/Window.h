//
// Created by jglrxavpok on 17/09/2021.
//

#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <engine/io/Resource.h>
#include <engine/Configuration.h>

namespace Carrot {
    class Engine;

    /// Represents the window in which the game will be rendered.
    class Window {
    public:
        void set16x16Icon(const Carrot::IO::Resource& icon);
        void set32x32Icon(const Carrot::IO::Resource& icon);
        void set64x64Icon(const Carrot::IO::Resource& icon);
        void set128x128Icon(const Carrot::IO::Resource& icon);
        void setTitle(std::string_view title);
        void maximise();
        void setPosition(std::int32_t x, std::int32_t y);
        void setWindowSize(std::int32_t w, std::int32_t h);

    public:
        void getFramebufferSize(std::int32_t& w, std::int32_t& h) const;

        void getPosition(std::int32_t& x, std::int32_t& y) const;
        void getWindowSize(std::int32_t& w, std::int32_t& h) const;

        bool isMaximised() const;

        GLFWwindow* getGLFWPointer() {
            return glfwWindow;
        }

        const GLFWwindow* getGLFWPointer() const {
            return glfwWindow;
        }

        ~Window();

    private:
        struct Icon {
            std::uint32_t width;
            std::uint32_t height;
            Carrot::IO::Resource image;
        };

        explicit Window(std::uint32_t width, std::uint32_t height, Configuration configuration);

        void setIcon(std::uint32_t size, const Carrot::IO::Resource& icon);
        void updateIcons();

    private:
        std::list<Icon> icons; // not a vector to avoid unnecessary copies. Is it really that big of a deal here? Probably not.
        GLFWwindow* glfwWindow = nullptr;

        friend class Carrot::Engine;
    };
}
