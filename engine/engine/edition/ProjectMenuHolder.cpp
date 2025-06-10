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

        shortcuts.add(cut);
        shortcuts.add(copy);
        shortcuts.add(duplicate);
        shortcuts.add(paste);
        shortcuts.activate();
    }

    void ProjectMenuHolder::handleShortcuts(const Carrot::Render::Context& frame) {
        if (cut.wasJustPressed()) {
            onCutShortcut(frame);
        } else if (copy.wasJustPressed()) {
            onCopyShortcut(frame);
        } else if (duplicate.wasJustPressed()) {
            onDuplicateShortcut(frame);
        } else if (paste.wasJustPressed()) {
            onPasteShortcut(frame);
        }
    }

    void ProjectMenuHolder::onCutShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onCopyShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onPasteShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onDuplicateShortcut(const Carrot::Render::Context& frame) {}

}
