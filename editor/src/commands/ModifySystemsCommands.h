//
// Created by jglrxavpok on 26/02/2025.
//

#pragma once

#include <commands/UndoStack.h>
#include <core/io/Document.h>

namespace Peeler {

    class AddSystemCommand: public ICommand {
    public:
        AddSystemCommand(Application& app, std::string systemName, bool isRender);

        void undo() override;
        void redo() override;

    private:
        std::string systemName;
        bool isRenderSystem = false;
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
