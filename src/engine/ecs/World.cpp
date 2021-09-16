//
// Created by jglrxavpok on 20/02/2021.
//

#include "World.h"
#include "systems/System.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "EntityTypes.h"

#include <engine/console/RuntimeOption.hpp>

Carrot::EasyEntity Carrot::World::newEntity(std::string_view name) {
    return newEntityWithID(allocationStrategy(), name);
}

Carrot::EasyEntity Carrot::World::newEntityWithID(EntityID id, std::string_view name) {
    auto entity = std::make_shared<Entity>(id);
    auto toReturn = EasyEntity(entity, *this);
    entityNames[id] = name;
    entitiesToAdd.emplace_back(entity);
    return toReturn;
}

Carrot::EasyEntity& Carrot::EasyEntity::addTag(Carrot::Tags tag) {
    worldRef.entityTags[*internalEntity] |= tag;
    return *this;
}

Carrot::Tags Carrot::EasyEntity::getTags() const {
    return worldRef.getTags(internalEntity);
}

Entity_Ptr Carrot::EasyEntity::getParent() {
    return worldRef.getParent(internalEntity);
}

const Entity_Ptr Carrot::EasyEntity::getParent() const {
    return worldRef.getParent(internalEntity);
}

void Carrot::EasyEntity::setParent(const Entity_Ptr& parent) {
    worldRef.setParent(internalEntity, parent);
}

std::string_view Carrot::EasyEntity::getName() const {
    return worldRef.getName(internalEntity);
}

void Carrot::EasyEntity::updateName(std::string_view name) {
    worldRef.entityNames[*internalEntity] = name;
}

Carrot::World::World() {
    EntityID entityID = 0;
    allocationStrategy = [entityID]() mutable {
        return entityID++;
    };
}

void Carrot::World::setAllocationStrategy(const std::function<EntityID()>& allocationStrategy) {
    this->allocationStrategy = allocationStrategy;
}

Carrot::Tags Carrot::World::getTags(const Entity_Ptr& entity) const {
    auto it = entityTags.find(*entity);
    if(it != entityTags.end()) {
        return it->second;
    }
    return {};
}

std::vector<Entity_Ptr> Carrot::World::getEntitiesWithTags(Tags tags) const {
    std::vector<Entity_Ptr> result;
    for(const auto& entity : entities) {
        if((getTags(entity) & tags) == tags) {
            result.push_back(entity);
        }
    }
    return result;
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
    if(!entitiesToRemove.empty()) {
        for(const auto& logic : logicSystems) {
            logic->onEntitiesRemoved(entitiesToRemove);
        }
        for(const auto& render : renderSystems) {
            render->onEntitiesRemoved(entitiesToRemove);
        }

        for(const auto& toRemove : entitiesToRemove) {
            auto position = find(entities.begin(), entities.end(), toRemove);
            if(position != entities.end()) { // clear components
                entityComponents.erase(entityComponents.find(*toRemove));
                entities.erase(position, entities.end());
            }


            setParent(toRemove, nullptr);
            entityChildren.erase(*toRemove);
            entityNames.erase(*toRemove);
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

static Carrot::RuntimeOption showWorldHierarchy("Debug/Show World hierarchy", false);

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

    {
        ZoneScopedN("Debug");
        if(showWorldHierarchy) {
            if(ImGui::Begin("World hierarchy", &showWorldHierarchy.getValueRef())) {
                std::function<void(const Entity_Ptr&)> showEntityTree = [&](const Entity_Ptr& entity) {
                    if(!entity)
                        return;
                    auto childrenIt = entityChildren.find(*entity);
                    if(childrenIt != entityChildren.end()) {
                        if(ImGui::TreeNode("##child", "%s", getName(entity).c_str())) {
                            auto& children = childrenIt->second;
                            for(const auto& c : children) {
                                showEntityTree(c);
                            }

                            ImGui::TreePop();
                        }
                    } else { // has no children
                        ImGui::Text("- %s", getName(entity).c_str());
                    }
                };
                for(const auto& ent : entities) {
                    if( ! getParent(ent)) {
                        showEntityTree(ent);
                    }
                }
            }
            ImGui::End();
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

Entity_Ptr Carrot::World::getParent(const Entity_Ptr& of) const {
    auto it = entityParents.find(*of);
    if(it != entityParents.end()) {
        return it->second;
    }
    return nullptr;
}

void Carrot::World::setParent(const Entity_Ptr& toSet, const Entity_Ptr& parent) {
    assert(toSet != nullptr);
    auto previousParent = entityParents[*toSet];
    if(previousParent) {
        auto& parentChildren = entityChildren[*previousParent];
        parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), toSet), parentChildren.end());
    }
    if(parent) {
        entityParents[*toSet] = parent;
        entityChildren[*parent].push_back(toSet);
    } else {
        entityParents.erase(*toSet);
    }
}

const std::vector<Entity_Ptr>& Carrot::World::getChildren(const Entity_Ptr& parent) const {
    auto it = entityChildren.find(*parent);
    if(it != entityChildren.end()) {
        return it->second;
    }
    static std::vector<Entity_Ptr> empty;
    return empty;
}

void Carrot::World::removeEntity(const Entity_Ptr& ent) {
    entitiesToRemove.push_back(ent);
    auto childrenCopy = entityChildren[*ent];
    for(const auto& child : childrenCopy) {
        removeEntity(child);
    }
}

const std::string& Carrot::World::getName(const Entity_Ptr& entity) const {
    auto it = entityNames.find(*entity);
    if(it != entityNames.end()) {
        return it->second;
    }
    throw std::runtime_error("Non-existent entity.");
}
