//
// Created by jglrxavpok on 26/02/2025.
//

#include "ModifySystemsCommands.h"

#include <Peeler.h>

namespace Peeler {
    AddSystemCommand::AddSystemCommand(Application& app, std::string systemName, bool isRender): ICommand(app, Carrot::sprintf("Add system %s", systemName.c_str())), systemName(std::move(systemName)), isRenderSystem(isRender) {}

    void AddSystemCommand::redo() {
        if (!Carrot::ECS::getSystemLibrary().has(systemName)) {
            return;
        }
        auto pSystem = Carrot::ECS::getSystemLibrary().create(systemName, editor.currentScene.world);
        if (isRenderSystem) {
            editor.currentScene.world.addRenderSystem(std::move(pSystem));
        } else {
            editor.currentScene.world.addLogicSystem(std::move(pSystem));
        }
    }

    void AddSystemCommand::undo() {
        if (isRenderSystem) {
            editor.currentScene.world.removeRenderSystem(editor.currentScene.world.getRenderSystem(systemName));
        } else {
            editor.currentScene.world.removeLogicSystem(editor.currentScene.world.getLogicSystem(systemName));
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