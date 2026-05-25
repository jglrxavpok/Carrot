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
     * A command that will apply the list of given commands when undone/redone
     * Appears as a single command to the user
     */
    class CompoundCommand final : public ICommand {
    public:
        CompoundCommand(Application& app) = delete;

        template<typename... TCommand>
        explicit CompoundCommand(Application& app, Carrot::UniquePtr<TCommand>... subcommand): ICommand(app, makeDescription(subcommand...)) {
            i32 count = 0;
            ((
                DISCARD(subcommand), count++
            ), ...);

            commands.resize(count);
            i32 index = 0;
            ((
                commands[index] = std::move(subcommand), index++
            ), ...);
        }

        void undo() override;
        void redo() override;

    private:
        template<typename... TCommand>
        static std::string makeDescription(const Carrot::UniquePtr<TCommand>&... command) {
            std::string desc = "Compound of ";
            (
                (desc += command->getDescription(), desc += " ")
            , ...);
            return desc;
        }

        Carrot::Vector<Carrot::UniquePtr<ICommand>> commands;
    };

    /**
     * Represents the stack of actions done by user, which can be undone, then redone.
     * If actions are undone and a *new* action is done, then all undone actions are lost.
     */
    class UndoStack {
    public:
        explicit UndoStack(Application& app);

        template<typename TCommand, typename... TArgs>
        TCommand& push(TArgs&&... args) {
            auto pCommand = Carrot::makeUnique<TCommand>(Carrot::Allocator::getDefault(), editor, std::forward<TArgs>(args)...);
            TCommand& command = *pCommand;
            internalPush(std::move(pCommand));
            return command;
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

        /**
         * Removes the entire contents of the undo stack
         */
        void clear();

    private:
        void internalPush(Carrot::UniquePtr<ICommand> command);

        Application& editor;
        std::int64_t cursor = 0; // cursor represent the index of the current state. That is +1 after the last not-undone command
        Carrot::Vector<Carrot::UniquePtr<ICommand>> stack;
    };

} // Peeler
