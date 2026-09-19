//
// Created by jglrxavpok on 19/09/2026.
//

#pragma once

namespace Carrot::Render {
    struct StencilSettings;
}

namespace Peeler {
    // Returns true if 'settings' or 'enabled' is modified
    bool editStencilSettings(Carrot::Render::StencilSettings& settings, bool& enabled);
}
