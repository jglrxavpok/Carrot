//
// Created by jglrxavpok on 04/06/25.
//

#include <engine/edition/ProjectMenuHolder.h>

namespace Tools {
    void ProjectMenuHolder::initShortcuts() {
        // TODO: base on settings
        cut.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_CONTROL) + Carrot::IO::GLFWKeyBinding(GLFW_KEY_X));

        shortcuts.add(cut);
        shortcuts.add(copy);
        shortcuts.add(duplicate);
        shortcuts.add(paste);
        shortcuts.activate();
    }

    void ProjectMenuHolder::handleShortcuts(const Carrot::Render::Context& frame) {
        if (cut.wasJustReleased()) {
            onCutShortcut(frame);
        } else if (copy.wasJustReleased()) {
            onCopyShortcut(frame);
        } else if (duplicate.wasJustReleased()) {
            onDuplicateShortcut(frame);
        } else if (paste.wasJustReleased()) {
            onPasteShortcut(frame);
        }
    }

    void ProjectMenuHolder::onCutShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onCopyShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onPasteShortcut(const Carrot::Render::Context& frame) {}

    void ProjectMenuHolder::onDuplicateShortcut(const Carrot::Render::Context& frame) {}

}
