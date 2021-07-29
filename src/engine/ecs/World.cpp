//
// Created by jglrxavpok on 20/02/2021.
//

#include "World.h"
#include "systems/System.h"
#include "engine/vulkan/CustomTracyVulkan.h"

Carrot::World::EasyEntity Carrot::World::newEntity() {
    auto newID = freeEntityID++;
    auto entity = make_shared<Entity>(newID);
    auto toReturn = EasyEntity(entity, *this);
    entitiesToAdd.emplace_back(entity);
    return toReturn;
}

void Carrot::World::tick(double dt) {
    for(const auto& toAdd : entitiesToAdd) {
        entities.push_back(toAdd);
    }
    if(!entitiesToAdd.empty()) {
        for(const auto& logic : logicSystems) {
            logic->onEntitiesAdded(entitiesToAdd);
        }
        for(const auto& render : renderSystems) {
            render->onEntitiesAdded(entitiesToAdd);
        }
    }
    for(const auto& toRemove : entitiesToRemove) {
        auto position = find(entities.begin(), entities.end(), toRemove);
        if(position != entities.end()) { // clear components
            entityComponents.erase(entityComponents.find(*toRemove));
        }
    }
    if(!entitiesToRemove.empty()) {
        for(const auto& logic : logicSystems) {
            logic->onEntitiesRemoved(entitiesToRemove);
        }
        for(const auto& render : renderSystems) {
            render->onEntitiesRemoved(entitiesToRemove);
        }
    }
    entitiesToAdd.clear();
    entitiesToRemove.clear();

    for(const auto& logic : logicSystems) {
        logic->tick(dt);
    }

    for(const auto& render : renderSystems) {
        render->tick(dt);
    }
}

void Carrot::World::onFrame(Carrot::Render::Context renderContext) {
    ZoneScoped;
    {
        ZoneScopedN("Logic");
        for(const auto& logic : logicSystems) {
            logic->onFrame(renderContext);
        }
    }

    {
        ZoneScopedN("Prepare render");
        for(const auto& render : renderSystems) {
            render->onFrame(renderContext);
        }
    }
}

void Carrot::World::recordGBufferPass(vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    ZoneScoped;
    {
        ZoneScopedN("GBuffer Logic");
        for(const auto& logic : logicSystems) {
            logic->gBufferRender(pass, renderContext, commands);
        }
    }

    {
        ZoneScopedN("GBuffer render");
        for(const auto& render : renderSystems) {
            render->gBufferRender(pass, renderContext, commands);
        }
    }
}

Carrot::Signature Carrot::World::getSignature(const Entity_Ptr& entity) const {
    auto componentMapLocation = this->entityComponents.find(*entity);
    if(componentMapLocation == this->entityComponents.end()) {
        // no such entity
        return Signature{};
    }

    auto& componentMap = componentMapLocation->second;
    Signature s{};
    for(const auto& [id, _] : componentMap) {
        s.addComponent(id);
    }
    return s;
}
