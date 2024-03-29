//
// Created by jglrxavpok on 18/03/2024.
//

#pragma once
#include <core/UniquePtr.hpp>
#include <core/containers/Vector.hpp>

namespace Peeler {
    class Application;

    class ICommand {
    public:
        explicit ICommand(Application& app, const std::string& description);

        const std::string& getDescription() const;

        virtual void undo() = 0;
        virtual void redo() = 0;
        virtual ~ICommand() = default;

    protected:
        Application& editor;

    private:
        std::string description;
    };

    /**
     * Represents the stack of actions done by user, which can be undone, then redone.
     * If actions are undone and a *new* action is done, then all undone actions are lost.
     */
    class UndoStack {
    public:
        explicit UndoStack(Application& app);

        template<typename TCommand, typename... TArgs>
        void push(TArgs&&... args) {
            auto pCommand = Carrot::makeUnique<TCommand>(Carrot::Allocator::getDefault(), editor, std::forward<TArgs>(args)...);
            internalPush(std::move(pCommand));
        }

        /**
         * Attempts to undo a single command.
         * Returns true iif there was a command to undo.
         */
        bool undo();

        /**
         * Attempts to redo a single command.
         * Returns true iif there was a command to redo.
         */
        bool redo();

        /**
         * Draws an ImGui window which can be used for debugging the undo stack
         */
        void debugDraw();

    private:
        void internalPush(Carrot::UniquePtr<ICommand> command);

        Application& editor;
        std::int64_t cursor = 0; // cursor represent the index of the current state. That is +1 after the last not-undone command
        Carrot::Vector<Carrot::UniquePtr<ICommand>> stack;
    };

} // Peeler
