//
// Created by jglrxavpok on 17/09/2023.
//

#pragma once

#include <engine/assets/Asset.h>
#include <core/data/Hashes.h>
#include <core/io/vfs/VirtualFileSystem.h>
#include <engine/render/AsyncResource.hpp>
#include <engine/ecs/EntityTypes.h>

namespace Carrot {
    struct Model;
    struct Pipeline;

    namespace Render {
        struct Context;
        struct Font;
        struct Texture;
    }

    /**
     * The engine/game asks for resources to this server, and the server returns an engine-optimized version of the wanted asset
     * This means if no up-to-date optimized version exists, a new one is created on the fly.
     */
    class AssetServer {
    public:
        explicit AssetServer(IO::VirtualFileSystem& vfs);
        ~AssetServer();

        void freeupResources(); //this is separate from the destructor because there are weird dependencies between AssetServer and VulkanRenderer

        void tick(double deltaTime);
        void beginFrame(const Carrot::Render::Context& renderContext);
        void beforeRecord(const Carrot::Render::Context& renderContext);

    public: // tmp stuff for migration
        bool TMLhasReloadedShaders();

    public:
        // TODO: request sound effect
        // TODO: request music
        // TODO: request animated model
        // TODO: request font
        // TODO: request rendering pipeline
        // TODO: move asset loading from VulkanRenderer to here
        std::shared_ptr<Model> loadModel(const Carrot::IO::VFS::Path& path);
        Async::Task<std::shared_ptr<Model>> coloadModel(Carrot::IO::VFS::Path path);

        std::shared_ptr<Render::Texture> loadTexture(const Carrot::IO::VFS::Path& path);
        Async::Task<std::shared_ptr<Render::Texture>> coloadTexture(Carrot::IO::VFS::Path path);

        std::shared_ptr<Pipeline> loadPipeline(const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset = 0);
        Async::Task<std::shared_ptr<Pipeline>> coloadPipeline(Carrot::IO::VFS::Path path, std::uint64_t instanceOffset = 0);

    public:
        std::int64_t getCurrentlyLoadingCount() const;

    private:
        void indexAssets();
        void dumpAssetReferences();

    private:
        IO::VirtualFileSystem& vfs;
        std::atomic_int64_t loadingCount{0};

        Async::ParallelMap<std::pair<std::string, std::uint64_t>, std::shared_ptr<Pipeline>> pipelines{};
        Async::ParallelMap<std::string, std::shared_ptr<Render::Texture>> textures{};

        Async::ParallelMap<std::string, std::shared_ptr<Model>> models{}; // models reference materials, so needs to be after material system

        Async::ParallelMap<std::string, std::shared_ptr<Render::Font>> fonts{};

    private: // migration stuff
        bool hasReloadedShadersThisFrame = false;
        friend class VulkanRenderer; // for transition period
    };

} // Carrot
