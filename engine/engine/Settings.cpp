//
// Created by jglrxavpok on 02/02/2025.
//

#include <core/Macros.h>
#include <engine/Settings.h>

namespace Carrot {
    Settings::Settings(int argc, char** argv) {
        for (int i = 0; i < argc; i++) {
            std::string_view s { argv[i] };
            if (s == "-nolivepp") {
                useCppHotReloading = false;
            }
            if (s == "-noaftermath") {
                useAftermath = false;
            }
            if (s == "-singlevkqueue") {
                singleQueue = true;
            }
        }
    }
}