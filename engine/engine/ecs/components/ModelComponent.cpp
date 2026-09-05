//
// Created by jglrxavpok on 08/10/2021.
//

#include "ModelComponent.h"
#include "engine/Engine.h"
#include "engine/render/ModelRenderer.h"
#include "engine/ecs/World.h"
#include "engine/ecs/WorldData.h"
#include <string>
#include <core/io/DocumentHelpers.h>

namespace Carrot::ECS {
    Carrot::DocumentElement ReflectedSerialisation<std::shared_ptr<Render::ModelRenderer>>::serialiseElement(const ECS::Component& component, const std::shared_ptr<Carrot::Render::ModelRenderer>& input) {
        Carrot::DocumentElement doc;
        if (input) {
            doc = input->uuid.toString();
        } else {
            doc = Carrot::UUID::null().toString();
        }
        return doc;
    }

    void ReflectedSerialisation<std::shared_ptr<Render::ModelRenderer>>::deserialiseElement(ECS::Component& component, std::shared_ptr<Carrot::Render::ModelRenderer>& out, const Carrot::DocumentElement& doc) {
        if (!doc.isString())
            return;
        Carrot::UUID rendererID = Carrot::UUID::fromString(doc.getAsString());
        WorldData& worldData = component.getEntity().getWorld().getWorldData();
        std::shared_ptr<Render::ModelRenderer> renderer = worldData.loadModelRenderer(rendererID);
        if(renderer != nullptr) {
            out = renderer;
        }
    }

    std::unique_ptr<Component> ModelComponent::duplicate(const Entity& newOwner) const {
        modelResource.forceWait(); // TODO: move inside duplicate of handles
        std::unique_ptr<Component> pResult = ReflectionComponent<ModelComponent>::duplicate(newOwner);
        ModelComponent* result = static_cast<ModelComponent*>(pResult.get());
        result->rendererStorage = rendererStorage.clone();
        return pResult;
    }

    void ModelComponent::setFile(const IO::VFS::Path& path) {
        modelResource = std::move(AsyncModelResource(GetAssetServer().loadModelTask(path)));
        modelRenderer = nullptr;
        if(GetCapabilities().supportsRaytracing) {
            rendererStorage.resetTLAS();
        }
    }

    void ModelComponent::enableTLAS() {
        rendererStorage.enableTLAS();
    }

    void ModelComponent::disableTLAS() {
        rendererStorage.disableTLAS();
    }

    void ModelComponent::applyRewriteRules(Carrot::DocumentElement& doc) {
        doc.rename("isTransparent", "Transparent");
        doc.rename("color", "Color");
        doc.rename("castsShadows", "CastsShadows");

        if (doc.contains("model")) {
            auto& modelData = doc["model"];
            doc["Model"] = modelData["modelPath"];

            auto modelDataAsObj = modelData.getAsObject();
            if (auto iter = modelDataAsObj.find("model_renderer"); iter != modelDataAsObj.end()) {
                doc["ModelRendererOverrides"] = modelData["model_renderer"];
            }
            doc["ModelRendererOverrides"] = Carrot::UUID::null().toString(); // ensure field is present
        }

        doc.remove("model");
    }
}
