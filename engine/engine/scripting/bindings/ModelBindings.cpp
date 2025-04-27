//
// Created by jglrxavpok on 02/04/2025.
//

#include "core/scripting/csharp/Engine.h"
#include "engine/ecs/components/ModelComponent.h"
#include "engine/scripting/CSharpBindings.h"

namespace Carrot::Scripting {
    static CSharpBindings& instance() {
        return GetCSharpBindings();
    }

    void CSharpBindings::addModelBindingTypes() {
        LOAD_CLASS_NS("Carrot.Components", ModelComponent);
    }

    static glm::vec4 _GetColor(MonoObject* comp) {
        auto& inst = instance();
        auto ownerEntity = inst.ComponentOwnerField->get(Scripting::CSObject(comp));
        ECS::Entity entity = inst.convertToEntity(ownerEntity);
        return entity.getComponent<ECS::ModelComponent>()->color;
    }

    static void _SetColor(MonoObject* comp, glm::vec4 v) {
        auto& inst = instance();
        auto ownerEntity = inst.ComponentOwnerField->get(Scripting::CSObject(comp));
        ECS::Entity entity = inst.convertToEntity(ownerEntity);
        entity.getComponent<ECS::ModelComponent>()->color = v;
    }

    void CSharpBindings::addModelBindingMethods() {
        mono_add_internal_call("Carrot.Components.ModelComponent::_GetColor", (void*)_GetColor);
        mono_add_internal_call("Carrot.Components.ModelComponent::_SetColor", (void*)_SetColor);
    }
}
