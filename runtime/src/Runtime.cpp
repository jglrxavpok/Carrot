//
// Created by jglrxavpok on 29/10/2023.
//

#include <engine/Engine.h>
#include <engine/Configuration.h>
#include <engine/CarrotGame.h>
#include <engine/scene/Scene.h>
#include <core/io/Logging.hpp>
#include <core/io/IO.h>
#include <engine/physics/Types.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/scripting/CSharpBindings.h>
#include <nfd.h>

static std::filesystem::path s_ProjectPath;

int main(int argc, char** argv) {
    if(argc > 1) {
        s_ProjectPath = argv[1];
    } else {
        NFD_Init();
        nfdchar_t* outPath;

        // prepare filters for the dialog
        nfdfilteritem_t filterItem[1] = {{"Project", "json"}};

        // show the dialog
        nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, nullptr);
        if (result == NFD_OKAY) {
            s_ProjectPath = outPath;
            // remember to free the memory (since NFD_OKAY is returned)
            NFD_FreePath(outPath);
        } else if (result == NFD_CANCEL) {
            // no-op
            return 1;
        } else {
            std::string msg = "Error: ";
            msg += NFD_GetError();
            throw std::runtime_error(msg);
        }
    }

    Carrot::Configuration config {
            .raytracingSupport = Carrot::RaytracingSupport::Supported,
            .applicationName = "Runtime",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .enableFileWatching = false,
            .startInFullscreen = true,
    };

    Carrot::Engine engine { config };
    Carrot::Settings settings = engine.getSettings();
    settings.fpsLimit = 30;
    engine.changeSettings(settings);
    engine.run();

    return 0;
}

class RuntimeGame: public Carrot::CarrotGame {
public:
    RuntimeGame(Carrot::Engine& engine): Carrot::CarrotGame(engine) {
        // TODO: deduplicate from Peeler.cpp
        const auto& projectToLoad = s_ProjectPath;
        GetVFS().addRoot("game", std::filesystem::absolute(projectToLoad).parent_path());
        rapidjson::Document description;
        try {
            description.Parse(Carrot::IO::readFileAsText(projectToLoad.string()).c_str());
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Failed to load project %s: %s", Carrot::toString(projectToLoad.u8string().c_str()).c_str(), e.what());
            return;
        }

        auto& layersManager = GetPhysics().getCollisionLayers();
        layersManager.reset();
        if(description.HasMember("collision_layers")) {
            std::unordered_map<std::string, Carrot::Physics::CollisionLayerID> name2layer;
            name2layer[layersManager.getLayer(layersManager.getStaticLayer()).name] = layersManager.getStaticLayer();
            name2layer[layersManager.getLayer(layersManager.getMovingLayer()).name] = layersManager.getMovingLayer();
            for(const auto& [key, obj] : description["collision_layers"].GetObject()) {
                auto layerID = layersManager.newLayer(std::string{ key.GetString(), key.GetStringLength() }, obj["static"].GetBool());
                name2layer[key.GetString()] = layerID;
            }

            for(const auto& [key, obj] : description["collision_layers"].GetObject()) {
                if(obj.HasMember("does_not_collide_with")) {
                    for(const auto& otherLayerName : obj["does_not_collide_with"].GetArray()) {
                        auto it = name2layer.find(std::string{ otherLayerName.GetString(), otherLayerName.GetStringLength() });
                        if(it == name2layer.end()) {
                            Carrot::Log::warn("Unknown collision layer: %s, when parsing collides_with of %s", otherLayerName.GetString(), key.GetString());
                            continue;
                        }

                        layersManager.setCanCollide(name2layer[key.GetString()], it->second, false);
                    }
                }
            }
        }

        // reloadGameAssembly:
        const Carrot::IO::VFS::Path root = "game://code";
        std::string dllName = getCurrentProjectName() + ".dll";

        const Carrot::IO::VFS::Path gameDll = root / std::string_view{"bin"} / dllName;

        if(!GetVFS().exists(gameDll)) {
            Carrot::Log::error("Game DLL not found!!");
        } else {
            GetCSharpBindings().loadGameAssembly(gameDll);
        }

        {
            openScene(description["scene"].GetString());
        }
    }

    void openScene(const std::string& sceneName) {
        rapidjson::Document sceneDoc;
        try {
            Carrot::IO::Resource sceneData = sceneName;
            sceneDoc.Parse(sceneData.readText());
            scene.clear();
            scene.deserialise(sceneDoc);
        } catch (std::exception& e) {
            Carrot::Log::error("Failed to open scene: %s", e.what());
            scene.clear();
        }

        scene.world.unfreezeLogic();
        scene.world.broadcastStartEvent();
        scene.world.tick(0.0); // force systems to update their entity lists
        scene.load();
        GetPhysics().resume();
    }

    void setupCamera(Carrot::Render::Context renderContext) override {
        scene.setupCamera(renderContext);
    }

    void prePhysics() override {
        scene.prePhysics();
    }

    void postPhysics() override {
        scene.postPhysics();
    }

    void onFrame(Carrot::Render::Context renderContext) {
        scene.onFrame(renderContext);
    }

    void tick(double frameTime) {
        scene.tick(frameTime);
    }

    std::string getCurrentProjectName() {
        return Carrot::toString(s_ProjectPath.stem().u8string());
    }

private:
    Carrot::Scene scene;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<RuntimeGame>(*this);
}
