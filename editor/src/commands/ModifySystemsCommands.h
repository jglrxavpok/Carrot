//
// Created by jglrxavpok on 26/02/2025.
//

#pragma once

#include <commands/UndoStack.h>
#include <core/io/Document.h>

namespace Peeler {

    class AddSystemsCommand: public ICommand {
    public:
        AddSystemsCommand(Application& app, Carrot::Vector<std::string> systemNames, bool areRenderSystems);

        void undo() override;
        void redo() override;

    private:
        Carrot::Vector<std::string> systemNames;
        bool areRenderSystems = false;
    };

    class RemoveSystemCommand: public ICommand {
    public:
        RemoveSystemCommand(Application& app, std::string systemName, bool isRender);

        void undo() override;
        void redo() override;

    private:
        std::string systemName;
        Carrot::DocumentElement serializedCopy;
        bool isRenderSystem = false;
    };

} // Peeler
