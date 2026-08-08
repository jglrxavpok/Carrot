#include "PrefabInstanceComponent.h"

#include <engine/assets/AssetServer.h>

namespace Carrot::ECS {
    void PrefabInstanceComponent::applyRewriteRules(Carrot::DocumentElement& doc) {
        doc.rename("prefab_path", "Prefab");
        doc.rename("child_id", "Child ID");
    }
} // Carrot::ECS