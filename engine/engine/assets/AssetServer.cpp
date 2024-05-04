//
// Created by jglrxavpok on 17/09/2023.
//

#include "AssetServer.h"
#include <engine/console/Console.h>
#include <engine/render/resources/Pipeline.h>
#include <engine/utils/Profiling.h>
#include <core/io/Logging.hpp>
#include <core/io/vfs/VirtualFileSystem.h>
#include <engine/Engine.h>
#include <core/io/FileSystemOS.h>
#include <Fertilizer.h>
#include <engine/ecs/Prefab.h>

namespace Carrot {
    namespace fs = std::filesystem;

    class AssetConversionException: public std::exception {
    public:
        std::string fullMessage;

        AssetConversionException(const Carrot::IO::VFS::Path& assetPath, const std::string& message) {
            fullMessage = Carrot::sprintf("AssetConversion failed: %s (%s)", message.c_str(), assetPath.toString().c_str());
        }

        const char *what() const noexcept override {
            return fullMessage.c_str();
        }
    };

    AssetServer::AssetServer(IO::VFS& vfs): vfs(vfs) {
        Console::instance().registerCommand("DumpAssetReferences", [this](Carrot::Engine& engine) {
            dumpAssetReferences();
        });

        vfsRoot = Carrot::IO::getExecutablePath().parent_path() / "asset_server";
        if(!std::filesystem::exists(vfsRoot)) {
            std::filesystem::create_directories(vfsRoot);
        }
        GetVFS().addRoot("asset_server", vfsRoot);
    }

    AssetServer::~AssetServer() {}

    void AssetServer::freeupResources() {
        pipelines.clear();
        textures.clear();
        models.clear();
        fonts.clear();
    }

    void AssetServer::tick(double deltaTime) {
        // TODO
    }

    void AssetServer::beginFrame(const Carrot::Render::Context& renderContext) {
        hasReloadedShadersThisFrame = false;
        for(auto& [name, pipePtrPtr] : pipelines.snapshot()) {
            hasReloadedShadersThisFrame |= (*pipePtrPtr)->checkForReloadableShaders();
        }
    }

    void AssetServer::beforeRecord(const Carrot::Render::Context& renderContext) {

    }

    bool AssetServer::TMLhasReloadedShaders() {
        return hasReloadedShadersThisFrame;
    }

    std::shared_ptr<Model> AssetServer::asyncLoadModel(TaskHandle& task, const Carrot::IO::VFS::Path& path) {
        Carrot::IO::Resource from;
        const std::string modelPath = path.toString();
        try {
            fs::path convertedPath = convert(path);
            if(convertedPath.empty()) {
                from = modelPath; // probably won't work, but at least the error message will be readable
            } else {
                from = Carrot::IO::Resource{ path, convertedPath };
            }
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Failed to load model %s", modelPath.c_str());
            // in case file could not be opened
            from = "resources/models/simple_cube.obj";
        } catch(AssetConversionException& e) {
            Carrot::Log::error("Could not import '%s': %s", modelPath.c_str(), e.what());
            // in case file could not be opened
            from = "resources/models/simple_cube.obj";
        }
        return Carrot::Model::load(task, GetEngine(), std::move(from));
    }

    std::shared_ptr<Model> AssetServer::blockingLoadModel(const Carrot::IO::VFS::Path& path) {
        Async::Counter sync;
        ZoneScopedN("Loading model");
        const std::string modelPath = path.toString();
        ZoneText(modelPath.c_str(), modelPath.size());
        return models.getOrCompute(modelPath, [&]() {
            std::shared_ptr<Model> result;
            GetTaskScheduler().schedule(TaskDescription {
                    .name = "Load model " + path.toString(),
                    .task = [&](TaskHandle& task) {
                        result = asyncLoadModel(task, path);
                    },
                    .joiner = &sync,
            }, TaskScheduler::AssetLoading);
            sync.busyWait();
            return result;
        });
    }

    std::shared_ptr<Model> AssetServer::loadModel(TaskHandle& task, const Carrot::IO::VFS::Path& path) {
        ZoneScopedN("Loading model");
        const std::string modelPath = path.toString();
        ZoneText(modelPath.c_str(), modelPath.size());
        return models.getOrCompute(task.getFiberHandle(), modelPath, [&]() {
            return asyncLoadModel(task, path);
        });
    }

    AssetServer::LoadTaskProc<Model> AssetServer::loadModelTask(const Carrot::IO::VFS::Path& path) {
        // TODO
        return [this, path](TaskHandle& task) {
            return loadModel(task, path);
        };
    }

    void AssetServer::removeFromModelCache(const Carrot::IO::VFS::Path& vfsPath) {
        models.remove(vfsPath.toString());
    }


    std::shared_ptr<Render::Texture> AssetServer::blockingLoadTexture(const Carrot::IO::VFS::Path& path) {
        ZoneScopedN("Loading texture");
        const std::string textureName = path.toString();
        ZoneText(textureName.c_str(), textureName.size());
        return textures.getOrCompute(textureName, [&]() {
            loadingCount++;
            CLEANUP(loadingCount--);
            Carrot::IO::Resource from;
            try {
                fs::path convertedPath = convert(path);
                if(convertedPath.empty()) {
                    from = textureName; // probably won't work, but at least the error message will be readable
                } else {
                    from = Carrot::IO::Resource{ path, convertedPath };
                }
            } catch(std::runtime_error& e) {
                Carrot::Log::error("Could not open texture '%s'", textureName.c_str());
                // in case file could not be opened
                from = "resources/textures/default.png";
            } catch(AssetConversionException& e) {
                Carrot::Log::error("Could not import '%s': %s", textureName.c_str(), e.what());
                // in case file could not be opened
                from = "resources/textures/default.png";
            }

            return std::make_shared<Carrot::Render::Texture>(GetVulkanDriver(), std::move(from));
        });
    }

    AssetServer::LoadTaskProc<Render::Texture> AssetServer::loadTextureTask(const Carrot::IO::VFS::Path& path) {
        return [this, path](TaskHandle& task) {
            return loadTexture(task, path);
        };
    }

    std::shared_ptr<Render::Texture> AssetServer::loadTexture(TaskHandle& currentTask, const Carrot::IO::VFS::Path& path) {
        return blockingLoadTexture(path);
    }

    std::shared_ptr<Pipeline> AssetServer::blockingLoadPipeline(const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset) {
        ZoneScopedN("Loading pipeline");
        const std::string name = path.toString();
        ZoneText(name.c_str(), name.size());
        auto key = std::make_pair(name, instanceOffset);
        return pipelines.getOrCompute(key, [&]() {
            auto pipeline = std::make_shared<Pipeline>(GetVulkanDriver(), path);
            pipeline->name(name);
            return pipeline;
        });
    }

    AssetServer::LoadTaskProc<Pipeline> AssetServer::loadPipelineTask(const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset) {
        return [this, path, instanceOffset](TaskHandle& task) {
            return loadPipeline(task, path, instanceOffset);
        };
    }

    std::shared_ptr<Pipeline> AssetServer::loadPipeline(TaskHandle& currentTask, const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset) {
        return blockingLoadPipeline(path, instanceOffset);
    }

    std::shared_ptr<Carrot::Render::AnimatedModel::Handle> AssetServer::blockingLoadAnimatedModelInstance(const Carrot::IO::VFS::Path& path) {
        Async::Counter sync;
        ZoneScopedN("Loading animated model instance");
        const std::string modelPath = path.toString();
        ZoneText(modelPath.c_str(), modelPath.size());
        std::shared_ptr<Carrot::Render::AnimatedModel::Handle> result;
        GetTaskScheduler().schedule(TaskDescription {
                .name = "Load animated model instance " + path.toString(),
                .task = [&](TaskHandle& task) {
                    result = loadAnimatedModelInstance(task, path);
                },
                .joiner = &sync,
        }, TaskScheduler::AssetLoading);
        sync.busyWait();
        return result;
    }

    AssetServer::LoadTaskProc<Carrot::Render::AnimatedModel::Handle> AssetServer::loadAnimatedModelInstanceTask(const Carrot::IO::VFS::Path& path) {
        return [this, path](TaskHandle& task) {
            return loadAnimatedModelInstance(task, path);
        };
    }

    std::shared_ptr<Carrot::Render::AnimatedModel::Handle> AssetServer::loadAnimatedModelInstance(TaskHandle& currentTask, const Carrot::IO::VFS::Path& path) {
        auto pAnimatedModel = animatedModels.getOrCompute(path.toString(), [&]() {
            // load the corresponding model data
            std::shared_ptr<Carrot::Model> pModel = loadModel(currentTask, path);

            // create the Animated Model object
            return std::make_shared<Render::AnimatedModel>(pModel);
        });
        return pAnimatedModel->requestHandle();
    }

    std::shared_ptr<ECS::Prefab> AssetServer::blockingLoadPrefab(const Carrot::IO::VFS::Path& path) {
        Async::Counter sync;
        const std::string modelPath = path.toString();
        std::shared_ptr<Carrot::ECS::Prefab> result;
        GetTaskScheduler().schedule(TaskDescription {
                .name = "Load prefab " + path.toString(),
                .task = [&](TaskHandle& task) {
                    result = loadPrefab(task, path);
                },
                .joiner = &sync,
        }, TaskScheduler::AssetLoading);
        sync.busyWait();
        return result;
    }

    std::shared_ptr<ECS::Prefab> AssetServer::loadPrefab(Carrot::TaskHandle& currentTask, const Carrot::IO::VFS::Path& path) {
        return prefabs.getOrCompute(path.toString(), [&]() {
            auto pPrefab = ECS::Prefab::makePrefab();
            pPrefab->load(currentTask, path);
            return pPrefab;
        });
    }

    void AssetServer::indexAssets() {
        // TODO
    }

    void AssetServer::dumpAssetReferences() {
        // TODO
    }

    fs::path AssetServer::getConvertedPath(const Carrot::IO::VFS::Path& path) {
        Carrot::IO::VFS::Path p = GetVFS().complete(path);
        if(p.isEmpty()) {
            return {};
        }
        const fs::path root { p.getRoot() };
        const fs::path relativePath = root / p.getPath().get();
        // path of asset as if we had copied it to <vfsRoot>/<path.getRoot()>/<path.getPath()>
        // examples: <vfsRoot>/game/resources/my_texture.jpg
        //           <vfsRoot>/engine/resources/models/cube.gltf
        fs::path expectedPath = vfsRoot / relativePath;
        fs::path convertedPath = Fertilizer::makeOutputPath(expectedPath);
        return convertedPath;
    }

    fs::path AssetServer::convert(const Carrot::IO::VFS::Path& path) {
        fs::path convertedPath = getConvertedPath(path);
        if(convertedPath.empty()) {
            return {};
        }

        Carrot::Profiling::PrintingScopedTimer convertTimer{ Carrot::sprintf("Converting %s", path.toString().c_str()) };
        const fs::path diskPath = GetVFS().resolve(path);
        Fertilizer::ConversionResult result = Fertilizer::convert(diskPath, convertedPath, false);

        if(result.errorCode != Fertilizer::ConversionResultError::Success) {
            throw AssetConversionException(path, result.errorMessage);
        }

        return convertedPath;
    }

} // Carrot