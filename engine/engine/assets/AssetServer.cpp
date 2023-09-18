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

namespace Carrot {
    AssetServer::AssetServer(IO::VFS& vfs): vfs(vfs) {
        Console::instance().registerCommand("DumpAssetReferences", [this](Carrot::Engine& engine) {
            dumpAssetReferences();
        });

        auto assetServerPath = Carrot::IO::getExecutablePath() / "asset_server";
        if(!std::filesystem::exists(assetServerPath)) {
            std::filesystem::create_directories(assetServerPath);
        }
        GetVFS().addRoot("asset_server", assetServerPath);
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

    std::shared_ptr<Model> AssetServer::loadModel(const Carrot::IO::VFS::Path& path) {
        ZoneScopedN("Loading model");
        const std::string modelPath = path.toString();
        ZoneText(modelPath.c_str(), modelPath.size());
        return models.getOrCompute(modelPath, [&]() {

            Carrot::IO::Resource from;
            try {
                from = modelPath;
            } catch(std::runtime_error& e) {
                Carrot::Log::error("Failed to load model %s", modelPath.c_str());
                // in case file could not be opened
                from = "resources/models/simple_cube.obj";
            }
            return std::make_shared<Carrot::Model>(GetEngine(), std::move(from));
        });
    }

    Carrot::Async::Task<std::shared_ptr<Model>> AssetServer::coloadModel(
            /* not a ref because we need the string to be alive inside the coroutine*/ Carrot::IO::VFS::Path path) {
        try {
            auto result = loadModel(path);
            co_return result;
        } catch (...) {
            throw;
        }
    }

    std::shared_ptr<Render::Texture> AssetServer::loadTexture(const Carrot::IO::VFS::Path& path) {
        ZoneScopedN("Loading texture");
        const std::string textureName = path.toString();
        ZoneText(textureName.c_str(), textureName.size());
        return textures.getOrCompute(textureName, [&]() {
            loadingCount++;
            CLEANUP(loadingCount--);
            Carrot::IO::Resource from;
            try {
                from = textureName;
            } catch(std::runtime_error& e) {
                Carrot::Log::error("Could not open texture '%s'", textureName.c_str());
                // in case file could not be opened
                from = "resources/textures/default.png";
            }

            return std::make_shared<Carrot::Render::Texture>(GetVulkanDriver(), std::move(from));
        });
    }

    Carrot::Async::Task<Render::Texture::Ref> AssetServer::coloadTexture(
            /* not a ref because we need the string to be alive inside the coroutine*/ Carrot::IO::VFS::Path path) {
        try {
            auto result = loadTexture(path);
            co_return result;
        } catch (...) {
            throw;
        }
    }

    std::shared_ptr<Pipeline> AssetServer::loadPipeline(const Carrot::IO::VFS::Path& path, std::uint64_t instanceOffset) {
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

    Carrot::Async::Task<std::shared_ptr<Pipeline>> AssetServer::coloadPipeline(Carrot::IO::VFS::Path path, std::uint64_t instanceOffset) {
        try {
            auto result = loadPipeline(path);
            co_return result;
        } catch (...) {
            throw;
        }
    }

    std::int64_t AssetServer::getCurrentlyLoadingCount() const {
        return loadingCount.load();
    }

    void AssetServer::indexAssets() {
        // TODO
    }

    void AssetServer::dumpAssetReferences() {
        // TODO
    }

} // Carrot