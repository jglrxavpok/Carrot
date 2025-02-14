//
// Created by jglrxavpok on 12/02/2025.
//

#pragma once
#include <optional>
#include <core/utils/Flags.h>

namespace Carrot::Widgets {

    enum class MessageBoxIcon {
        Info,
        Warning,
        Error,
    };
    enum class MessageBoxButtons: u32 {
        Ok = 1 << 0,
        Yes = 1 << 1,
        Save = 1 << 2,
        DontSave = 1 << 3,
        No = 1 << 4,
        Cancel = 1 << 5,
    };
    ENUM_FLAGS_OPERATORS(MessageBoxButtons)

    /**
     * Draws a message box using ImGui. Intended to be called each frame where you want to show this message box.
     * Returns empty optional if the user has not selected an option yet. Otherwise returns the button that was pressed
     */
    std::optional<MessageBoxButtons> drawMessageBox(const char* title, const char* caption, MessageBoxIcon type, MessageBoxButtons buttons);

} // Carrot::Widgets
