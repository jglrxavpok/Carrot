//
// Created by jglrxavpok on 17/09/2023.
//

#pragma once

#include <core/data/Hashes.h>
#include <core/io/vfs/VirtualFileSystem.h>
#include <engine/render/AsyncResource.hpp>
#include <engine/ecs/EntityTypes.h>
#include <engine/task/TaskScheduler.h>
#include <engine/render/animation/AnimatedModel.h>

namespace Carrot {
    class Model;
    class Pipeline;

    namespace Render {
        struct Context;
        struct Font;
        class Texture;
    }

    namespace ECS {
        class Prefab;
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
        template<typename T>
        using LoadTaskProc = std::function<std::shared_ptr<T>(TaskHandle&)>;

        // TODO: request sound effect
        // TODO: request music
        // TODO: request font
        // TODO: request rendering pipeline
        // TODO: move asset loading from VulkanRenderer to here
        std::shared_ptr<Model> blockingLoadModel(const Carrot::IO::VFS::Path& path);
        LoadTaskProc<Model> loadModelTask(const Carrot::IO::VFS::Path& path);
        std::shared_ptr<Model> loadModel(Carrot::TaskHandle& currentTask, const Carrot::IO::VFS::Path& path);
        void removeFromModelCache(const Carrot::IO::VFS::Path& vfsPath);

        std::shared_ptr<Render::Texture> blockingLoadTexture(const Carrot::IO::VFS::Path& path);
        LoadTaskProc<Render::Texture> loadTextureTask(const Carrot::IO::VFS::Path& path);
        std::shared_ptr<Render::Texture> loadTexture(Carrot::TaskHandle& currentTask, const Carrot::IO::VFS::Path& path);

        std::shared_ptr<Pipeline> blockingLoadPipeline(const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset = 0);
        LoadTaskProc<Pipeline> loadPipelineTask(const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset = 0);
        std::shared_ptr<Pipeline> loadPipeline(Carrot::TaskHandle& currentTask, const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset = 0);

        std::shared_ptr<Carrot::Render::AnimatedModel::Handle> blockingLoadAnimatedModelInstance(const Carrot::IO::VFS::Path& path);
        LoadTaskProc<Carrot::Render::AnimatedModel::Handle> loadAnimatedModelInstanceTask(const Carrot::IO::VFS::Path& path);
        std::shared_ptr<Carrot::Render::AnimatedModel::Handle> loadAnimatedModelInstance(Carrot::TaskHandle& currentTask, const Carrot::IO::VFS::Path& path);

        std::shared_ptr<ECS::Prefab> blockingLoadPrefab(const Carrot::IO::VFS::Path& path);
        std::shared_ptr<ECS::Prefab> loadPrefab(Carrot::TaskHandle& currentTask, const Carrot::IO::VFS::Path& path);

        /// Intended for use by Prefab only: next call to loadPrefab with the VFS path will return the input prefab.
        /// This is meant to populate the asset server when a new prefab is created from the editor, so that further calls to loadPrefab find the new prefab.
        /// If a prefab already existed with the VFS path of the prefab, it will be overriden inside the asset server
        void storePrefab(ECS::Prefab& prefab);
        void removePrefab(const Carrot::IO::VFS::Path& path);

    public:
        std::int64_t getCurrentlyLoadingCount() const;

    private:
        void indexAssets();
        void dumpAssetReferences();

        std::shared_ptr<Model> asyncLoadModel(TaskHandle& task, const Carrot::IO::VFS::Path& path);
        std::filesystem::path getConvertedPath(const Carrot::IO::VFS::Path& path); // find the path inside asset_server folder for the converted asset
        std::filesystem::path convert(const Carrot::IO::VFS::Path& path); // performs conversion

    private:
        IO::VirtualFileSystem& vfs;
        std::filesystem::path vfsRoot;
        std::atomic_int64_t loadingCount{0};

        Async::ParallelMap<std::pair<std::string, std::uint64_t>, std::shared_ptr<Pipeline>> pipelines{};
        Async::ParallelMap<std::string, std::shared_ptr<Render::Texture>> textures{};

        Async::ParallelMap<std::string, std::shared_ptr<Model>> models{}; // models reference materials, so needs to be after material system

        Async::ParallelMap<std::string, std::shared_ptr<Render::Font>> fonts{};
        Async::ParallelMap<std::string, std::shared_ptr<Render::AnimatedModel>> animatedModels{};
        Async::ParallelMap<std::string, std::shared_ptr<ECS::Prefab>> prefabs{};

    private: // migration stuff
        bool hasReloadedShadersThisFrame = false;
        friend class VulkanRenderer; // for transition period
    };

} // Carrot
