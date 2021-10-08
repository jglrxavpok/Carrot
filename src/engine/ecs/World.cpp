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

    std::string& Entity::getName() {
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

    std::vector<Component*> Entity::getAllComponents() const {
        return worldRef.getAllComponents(*this);
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
            auto obj = wrap(entity);
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
                    entities.erase(position);
                }


                setParent(wrap(toRemove), {});
                entityChildren.erase(toRemove);
                entityNames.erase(toRemove);
            }
        }
        entitiesToAdd.clear();
        entitiesToRemove.clear();

        if(!frozenLogic) {
            for(const auto& logic : logicSystems) {
                logic->tick(dt);
            }
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
                        auto entityObj = wrap(ent);
                        if( ! getParent(entityObj)) {
                            showEntityTree(entityObj);
                        }
                    }
                }
                ImGui::End();
            }
        }
    }

    void World::recordOpaqueGBufferPass(const vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        ZoneScoped;
        {
            ZoneScopedN("GBuffer Logic");
            for(const auto& logic : logicSystems) {
                logic->opaqueGBufferRender(pass, renderContext, commands);
            }
        }

        {
            ZoneScopedN("GBuffer render");
            for(const auto& render : renderSystems) {
                render->opaqueGBufferRender(pass, renderContext, commands);
            }
        }
    }

    void World::recordTransparentGBufferPass(const vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        ZoneScoped;
        {
            ZoneScopedN("GBuffer Logic");
            for(const auto& logic : logicSystems) {
                logic->transparentGBufferRender(pass, renderContext, commands);
            }
        }

        {
            ZoneScopedN("GBuffer render");
            for(const auto& render : renderSystems) {
                render->transparentGBufferRender(pass, renderContext, commands);
            }
        }
    }

    Signature World::getSignature(const Entity& entity) const {
        auto componentMapLocation = this->entityComponents.find(entity);
        if(componentMapLocation == this->entityComponents.end()) {
            // no such entity
            return Signature{};
        }

        // TODO: might be interesting to pre-compute this value when entity components are modified
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
            return wrap(it->second);
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

    std::string& World::getName(const Entity& entity) {
        return getName(entity.getID());
    }

    const std::string& World::getName(const Entity& entity) const {
        return getName(entity.getID());
    }

    std::string& World::getName(const EntityID& entityID) {
        auto it = entityNames.find(entityID);
        if(it != entityNames.end()) {
            return it->second;
        }
        throw std::runtime_error("Non-existent entity.");
    }

    const std::string& World::getName(const EntityID& entityID) const {
        auto it = entityNames.find(entityID);
        if(it != entityNames.end()) {
            return it->second;
        }
        throw std::runtime_error("Non-existent entity.");
    }

    bool World::exists(EntityID ent) const {
        return entityNames.find(ent) != entityNames.end();
    }

    std::vector<Entity> World::getAllEntities() const {
        std::vector<Entity> list;
        for(const auto& ent : entities) {
            list.emplace_back(ent, const_cast<World&>(*this));
        }
        return list;
    }

    std::vector<Component *> World::getAllComponents(const Entity& entity) const {
        return getAllComponents(entity.getID());
    }


    std::vector<Component *> World::getAllComponents(const EntityID& entityID) const {
        std::vector<Component*> comps;
        for(auto& [id, comp] : entityComponents.at(entityID)) {
            comps.push_back(comp.get());
        }
        return comps;
    }

    Entity World::wrap(EntityID id) const {
        return Entity(id, const_cast<World&>(*this));
    }

    World& World::operator=(const World& toCopy) {
        entityParents = toCopy.entityParents;
        entityChildren = toCopy.entityChildren;
        entityNames = toCopy.entityNames;
        entityTags = toCopy.entityTags;
        entities = toCopy.entities;
        entitiesToAdd = toCopy.entitiesToAdd;
        entitiesToRemove = toCopy.entitiesToRemove;
        frozenLogic = toCopy.frozenLogic;

        auto copySystems = [&](std::vector<std::unique_ptr<System>>& dest, const std::vector<std::unique_ptr<System>>& src) {
            dest.clear();
            dest.resize(src.size());

            for (std::size_t i = 0; i < src.size(); ++i) {
                dest[i] = src[i]->duplicate(*this);
                for(const auto& srcEntity : src[i]->entities) {
                    dest[i]->entities.emplace_back(srcEntity.internalEntity, *this);
                }
            }
        };
        copySystems(logicSystems, toCopy.logicSystems);
        copySystems(renderSystems, toCopy.renderSystems);

        entityComponents.clear();
        for(const auto& [entityID, componentMap] : toCopy.entityComponents) {
            auto& destComponents = entityComponents[entityID];
            for(const auto& [id, comp] : componentMap) {
                destComponents[id] = comp->duplicate(wrap(entityID));
            }
        }

        return *this;
    }
}
