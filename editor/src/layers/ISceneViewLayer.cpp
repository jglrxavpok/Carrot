//
// Created by jglrxavpok on 20/02/2023.
//

#include "ISceneViewLayer.h"
#include "../Peeler.h"

namespace Peeler {
    void ISceneViewLayer::remove() {
        editor.removeLayer(this);
    }
}