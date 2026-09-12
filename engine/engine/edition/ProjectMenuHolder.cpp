//
// Created by jglrxavpok on 04/06/25.
//

#include <engine/edition/ProjectMenuHolder.h>

namespace Tools {
    void ProjectMenuHolder::initShortcuts() {
        // TODO: base on settings
        cut.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_X));
        copy.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_C));
        paste.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_V));
        duplicate.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_D));

        // TODO: use KEY_Z, but needs to always point to Z no matter the keyboard layout. I use an AZERTY keyboard so Control+W it is for now.
        undoShortcut.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_W));
        redoShortcut.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_Y));
        deleteShortcut.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_DELETE));

        shortcuts.add(cut);
        shortcuts.add(copy);
        shortcuts.add(duplicate);
        shortcuts.add(paste);
        shortcuts.add(deleteShortcut);
        shortcuts.add(undoShortcut);
        shortcuts.add(redoShortcut);
        shortcuts.activate();
    }

    void ProjectMenuHolder::handleShortcuts(const Carrot::Render::Context& frame) {
        if (cut.isPressed()) {
            if (!cutWasPressed) {
                onCutShortcut(frame);
            }
        } else if (copy.isPressed()) {
            if (!copyWasPressed) {
                onCopyShortcut(frame);
            }
        } else if (duplicate.isPressed()) {
            if (!duplicateWasPressed) {
                onDuplicateShortcut(frame);
            }
        } else if (paste.isPressed()) {
            if (!pasteWasPressed) {
                onPasteShortcut(frame);
            }
        } else if (deleteShortcut.isPressed()) {
            if (!deleteWasPressed) {
                onDeleteShortcut(frame);
            }
        } else if (undoShortcut.isPressed()) {
            if (!undoWasPressed) {
                onUndoShortcut(frame);
            }
        } else if (redoShortcut.isPressed()) {
            if (!redoWasPressed) {
                onRedoShortcut(frame);
            }
        }

        // not using wasJustPressed because we are in a frame render context, which may skip inputs (inputs wasJustPressed is reliable on tick contextes only)
        cutWasPressed = cut.isPressed();
        copyWasPressed = copy.isPressed();
        duplicateWasPressed = duplicate.isPressed();
        pasteWasPressed = paste.isPressed();
        deleteWasPressed = deleteShortcut.isPressed();
        undoWasPressed = undoShortcut.isPressed();
        redoWasPressed = redoShortcut.isPressed();
    }

    void ProjectMenuHolder::onCutShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onCopyShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onPasteShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onDuplicateShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onDeleteShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onUndoShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onRedoShortcut(const Carrot::Render::Context& frame) {}

}
