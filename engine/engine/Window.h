//
// Created by jglrxavpok on 17/09/2021.
//

#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <core/io/Resource.h>
#include <engine/Configuration.h>

namespace Carrot {
    class Engine;

    namespace Render {
        class Texture;
    }

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    using WindowID = std::int64_t;

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
        void setMainWindow();

        WindowID getWindowID() const;
        bool operator==(const Window& other) const;
        bool operator==(const WindowID& otherID) const;

    public:
        void getFramebufferSize(std::int32_t& w, std::int32_t& h) const;

        void getPosition(std::int32_t& x, std::int32_t& y) const;
        void getWindowSize(std::int32_t& w, std::int32_t& h) const;

        bool isMaximised() const;
        bool isMainWindow() const;

        GLFWwindow* getGLFWPointer() {
            return glfwWindow;
        }

        const GLFWwindow* getGLFWPointer() const {
            return glfwWindow;
        }

        vk::SurfaceKHR getSurface() const;

        ~Window();

    public:
        void computeSwapchainOffset(); /// we compute an offset between the main window and this windows's swapchain indices because the renderer believes all swapchains use the same index
        void createSurface();
        void createSwapChain();
        void cleanupSwapChain();
        void destroySwapchainAndSurface();
        SwapChainSupportDetails getSwapChainSupport(const vk::PhysicalDevice& device);

        void setCurrentSwapchainIndex(std::int32_t mainWindowSwapchainIndex, std::int32_t thisWindowSwapchainIndex);
        std::size_t getSwapchainIndexOffset() const; // offset of 'real' swapchain index compared to global swapchain index (which is swapchain index of main window)

        /**
         * True iif at least one call to acquireNextImageKHR was done for this frame.
         * Can be used to avoid presenting to a window & swapchain image that was initialized between acquireNextImage and present.
         */
        bool hasAcquiredAtLeastOneImage() const;
        bool isFirstPresent() const;
        void clearFirstPresent();

        vk::Semaphore getImageAvailableSemaphore(std::size_t frameInFlightIndex);

        size_t getSwapchainImageCount() const;
        const vk::Format& getSwapchainImageFormat() const;
        const vk::Extent2D& getFramebufferExtent() const;
        const vk::SwapchainKHR& getSwapchain() const;
        std::shared_ptr<Render::Texture> getSwapchainTexture(std::size_t swapchainIndex);

        void fetchNewFramebufferSize();

    public: // public for usage by STL, do not use! Use Engine::createWindow instead
        explicit Window(Engine& engine, std::uint32_t width, std::uint32_t height, Configuration configuration);
        explicit Window(Engine& engine, GLFWwindow* pWindow); //< adopt an externally-managed window
        explicit Window(Engine& engine, vk::SurfaceKHR surface); //< adopt an externally-managed surface (eg ImGui GLFW backend for instance)

    private:
        vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);
        /// Choose best surface format from the list of given formats
        /// \param formats
        /// \return
        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);

        /// Choose best present mode from the list of given modes
        /// \param presentModes
        /// \return
        vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes);

    private:
        struct Icon {
            std::uint32_t width;
            std::uint32_t height;
            Carrot::IO::Resource image;
        };

        explicit Window(Engine& engine); //< base constructor, should only be called by other constructors

        void setIcon(std::uint32_t size, const Carrot::IO::Resource& icon);
        void updateIcons();

    private:
        Carrot::Engine& engine;
        WindowID windowID = 0;
        std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
        std::list<Icon> icons; // not a vector to avoid unnecessary copies. Is it really that big of a deal here? Probably not.
        vk::SurfaceKHR surface;
        bool mainWindow = false;
        bool ownsWindow = false;
        bool ownsSurface = false;
        bool acquiredAtLeastOneImage = false;
        bool firstPresent = true;
        std::int32_t swapchainIndexOffset = -1;
        std::int32_t swapchainIndex = -1;
        GLFWwindow* glfwWindow = nullptr;
        SwapChainSupportDetails swapChainSupportDetails;
        vk::UniqueSwapchainKHR swapchain{};
        vk::Format swapchainImageFormat = vk::Format::eUndefined;
        vk::Extent2D framebufferExtent{};
        std::vector<std::shared_ptr<Render::Texture>> swapchainTextures{}; // will not own data because deleted with swapchain
    };
}
