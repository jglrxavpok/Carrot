//
// Created by jglrxavpok on 20/02/2021.
//

#include "World.h"
#include "systems/System.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "EntityTypes.h"

#include <engine/console/RuntimeOption.hpp>

namespace Carrot::ECS {
    Entity World::newEntity(std::string_view name) {
        Carrot::UUID uuid;
        return newEntityWithID(uuid, name);
    }

    Entity World::newEntityWithID(EntityID entity, std::string_view name) {
        auto toReturn = Entity(entity, *this);
        entityNames[entity] = name;
        entitiesToAdd.emplace_back(toReturn);
        return toReturn;
    }

    Entity& Entity::addTag(Tags tag) {
        worldRef.entityTags[internalEntity] |= tag;
        return *this;
    }

    Tags Entity::getTags() const {
        return worldRef.getTags(*this);
    }

    std::optional<Entity> Entity::getParent() {
        return worldRef.getParent(*this);
    }

    std::optional<const Entity> Entity::getParent() const {
        return worldRef.getParent(*this);
    }

    void Entity::setParent(std::optional<Entity> parent) {
        worldRef.setParent(*this, parent);
    }

    std::string_view Entity::getName() const {
        return worldRef.getName(*this);
    }

    void Entity::updateName(std::string_view name) {
        worldRef.entityNames[internalEntity] = name;
    }

    Entity::operator bool() const {
        return exists();
    }

    bool Entity::exists() const {
        return worldRef.exists(internalEntity);
    }

    World::World() {}

    Tags World::getTags(const Entity& entity) const {
        auto it = entityTags.find(entity);
        if(it != entityTags.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<Entity> World::getEntitiesWithTags(Tags tags) const {
        std::vector<Entity> result;
        for(const auto& entity : entities) {
            auto obj = Entity(entity, const_cast<World&>(*this));
            if((getTags(obj) & tags) == tags) {
                result.push_back(obj);
            }
        }
        return result;
    }

    void World::tick(double dt) {
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
                    entityComponents.erase(entityComponents.find(toRemove));
                    entities.erase(position, entities.end());
                }


                setParent(Entity(toRemove, *this), {});
                entityChildren.erase(toRemove);
                entityNames.erase(toRemove);
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

    void World::onFrame(Carrot::Render::Context renderContext) {
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
                    std::function<void(Entity&)> showEntityTree = [&](Entity& entity) {
                        if(!entity)
                            return;
                        auto children = getChildren(entity);
                        if(!children.empty()) {
                            if(ImGui::TreeNode("##child", "%s", getName(entity).c_str())) {
                                for(auto& c : children) {
                                    showEntityTree(c);
                                }

                                ImGui::TreePop();
                            }
                        } else { // has no children
                            ImGui::Text("- %s", getName(entity).c_str());
                        }
                    };
                    for(const auto& ent : entities) {
                        auto entityObj = Entity(ent, *this);
                        if( ! getParent(entityObj)) {
                            showEntityTree(entityObj);
                        }
                    }
                }
                ImGui::End();
            }
        }
    }

    void World::recordGBufferPass(vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
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

    Signature World::getSignature(const Entity& entity) const {
        auto componentMapLocation = this->entityComponents.find(entity);
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

    std::optional<Entity> World::getParent(const Entity& of) const {
        auto it = entityParents.find(of);
        if(it != entityParents.end()) {
            return Entity(it->second, const_cast<World&>(*this));
        }
        return {};
    }

    void World::setParent(const Entity& toSet, std::optional<Entity> parent) {
        assert(toSet);
        auto previousParent = entityParents.find(toSet);
        if(previousParent != entityParents.end()) {
            auto& parentChildren = entityChildren[previousParent->second];
            parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), toSet.internalEntity), parentChildren.end());
        }
        if(parent) {
            entityParents[toSet] = parent.value();
            entityChildren[*parent].push_back(toSet);
        } else {
            entityParents.erase(toSet);
        }
    }

    const std::vector<Entity> World::getChildren(const Entity& parent) const {
        auto it = entityChildren.find(parent);
        if(it != entityChildren.end()) {
            std::vector<Entity> children;
            for(auto& entityID : it->second) {
                children.emplace_back(entityID, const_cast<World&>(*this));
            }
            return children;
        }
        static std::vector<Entity> empty;
        return empty;
    }

    void World::removeEntity(const Entity& ent) {
        entitiesToRemove.push_back(ent);
        auto childrenCopy = entityChildren[ent];
        for(const auto& child : childrenCopy) {
            removeEntity(Entity(child, *this));
        }
    }

    const std::string& World::getName(const Entity& entity) const {
        auto it = entityNames.find(entity);
        if(it != entityNames.end()) {
            return it->second;
        }
        throw std::runtime_error("Non-existent entity.");
    }

    bool World::exists(EntityID ent) const {
        return entityNames.find(ent) != entityNames.end();
    }

}
