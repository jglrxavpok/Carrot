//
// Created by jglrxavpok on 26/02/2025.
//

#include "ModifySystemsCommands.h"

#include <Peeler.h>

namespace Peeler {
    AddSystemsCommand::AddSystemsCommand(Application& app, Carrot::Vector<std::string> systemNames, bool areRenderSystems): ICommand(app, Carrot::sprintf("Add systems")), systemNames(std::move(systemNames)), areRenderSystems(areRenderSystems) {}

    void AddSystemsCommand::redo() {
        for (const auto& systemName : systemNames) {
            if (!Carrot::ECS::getSystemLibrary().has(systemName)) {
                return;
            }
            auto pSystem = Carrot::ECS::getSystemLibrary().create(systemName, editor.currentScene.world);
            if (areRenderSystems) {
                editor.currentScene.world.addRenderSystem(std::move(pSystem));
            } else {
                editor.currentScene.world.addLogicSystem(std::move(pSystem));
            }
        }
    }

    void AddSystemsCommand::undo() {
        for (const auto& systemName : systemNames) {
            if (areRenderSystems) {
                editor.currentScene.world.removeRenderSystem(editor.currentScene.world.getRenderSystem(systemName));
            } else {
                editor.currentScene.world.removeLogicSystem(editor.currentScene.world.getLogicSystem(systemName));
            }
        }
    }

    RemoveSystemCommand::RemoveSystemCommand(Application& app, std::string systemName, bool isRender): ICommand(app, Carrot::sprintf("Remove system %s", systemName.c_str())), systemName(std::move(systemName)), isRenderSystem(isRender) {
        const Carrot::ECS::System* pSystem;
        if (isRender) {
            pSystem = editor.currentScene.world.getRenderSystem(systemName);
        } else {
            pSystem = editor.currentScene.world.getLogicSystem(systemName);
        }
        if (pSystem) {
            serializedCopy = pSystem->serialise();
        }
    }

    void RemoveSystemCommand::redo() {
        if (isRenderSystem) {
            editor.currentScene.world.removeRenderSystem(editor.currentScene.world.getRenderSystem(systemName));
        } else {
            editor.currentScene.world.removeLogicSystem(editor.currentScene.world.getLogicSystem(systemName));
        }
    }

    void RemoveSystemCommand::undo() {
        if (!Carrot::ECS::getSystemLibrary().has(systemName)) {
            return;
        }
        auto pSystem = Carrot::ECS::getSystemLibrary().deserialise(systemName, serializedCopy, editor.currentScene.world);

        if (isRenderSystem) {
            editor.currentScene.world.addRenderSystem(std::move(pSystem));
        } else {
            editor.currentScene.world.addLogicSystem(std::move(pSystem));
        }
    }



} // Peeler