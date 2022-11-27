//
// Created by jglrxavpok on 20/02/2021.
//

#include "World.h"
#include "systems/System.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "EntityTypes.h"

#include "engine/console/RuntimeOption.hpp"
#include "engine/Engine.h"

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

    std::optional<Entity> Entity::getNamedChild(std::string_view name, ShouldRecurse recurse) {
        return worldRef.getNamedChild(name, *this, recurse);
    }

    std::optional<const Entity> Entity::getNamedChild(std::string_view name, ShouldRecurse recurse) const {
        return worldRef.getNamedChild(name, *this, recurse);
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

    void Entity::remove() {
        worldRef.removeEntity(*this);
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

    Memory::OptionalRef<Component> Entity::getComponent(ComponentID component) const {
        return worldRef.getComponent(*this, component);
    }

    Entity& Entity::removeComponent(Component& component) {
        return removeComponent(component.getComponentTypeID());
    }

    Entity& Entity::removeComponent(const ComponentID& componentID) {
        auto& componentMap = worldRef.entityComponents[internalEntity];
        componentMap.erase(componentID);
        worldRef.entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    Signature Entity::getSignature() const {
        return worldRef.getSignature(*this);
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

    void World::updateEntityLists() {
        if(!entitiesUpdated.empty()) {
            for(const auto& logic : logicSystems) {
                logic->onEntitiesUpdated(entitiesUpdated);
            }
            for(const auto& render : renderSystems) {
                render->onEntitiesUpdated(entitiesUpdated);
            }
        }
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
        updateEntityLists();
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
        entitiesUpdated.clear();

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

    void World::setupCamera(Carrot::Render::Context renderContext) {
        ZoneScoped;

        {
            ZoneScopedN("Systems setup their cameras");
            for(const auto& render : renderSystems) {
                ZoneScopedN("RenderSystem");
                ZoneText(typeid(*render).name(), std::strlen(typeid(*render).name()));
                render->setupCamera(renderContext);
            }
        }
    }

    void World::onFrame(Carrot::Render::Context renderContext) {
        ZoneScoped;

        updateEntityLists();
        {
            ZoneScopedN("Logic");
            for(const auto& logic : logicSystems) {
                ZoneScopedN("LogicSystem");
                ZoneText(typeid(*logic).name(), std::strlen(typeid(*logic).name()));
                logic->onFrame(renderContext);
            }
        }

        {
            ZoneScopedN("Prepare render");
            for(const auto& render : renderSystems) {
                ZoneScopedN("RenderSystem");
                ZoneText(typeid(*render).name(), std::strlen(typeid(*render).name()));
                render->onFrame(renderContext);
            }
        }

        {
            ZoneScopedN("Swap buffers");
            for(const auto& logic : logicSystems) {
                ZoneScopedN("LogicSystem");
                ZoneText(typeid(*logic).name(), std::strlen(typeid(*logic).name()));
                logic->swapBuffers();
            }
            for(const auto& render : renderSystems) {
                ZoneScopedN("RenderSystem");
                ZoneText(typeid(*render).name(), std::strlen(typeid(*render).name()));
                render->swapBuffers();
            }
        }

        {
            ZoneScopedN("Debug");
            if(showWorldHierarchy && &renderContext.viewport == &GetEngine().getMainViewport()) {
                if(ImGui::Begin("World hierarchy", &showWorldHierarchy.getValueRef())) {
                    std::function<void(Entity&)> showEntityTree = [&](Entity& entity) {
                        if(!entity)
                            return;
                        auto children = getChildren(entity, ShouldRecurse::NoRecursion);
                        if(ImGui::TreeNode("##child", "%s", getName(entity).c_str())) {
                            for(auto& c : children) {
                                showEntityTree(c);
                            }

                            ImGui::TreePop();
                        }
                    };
                    for(const auto& ent : entities) {
                        auto entityObj = wrap(ent);
                        if(!getParent(entityObj).has_value()) {
                            showEntityTree(entityObj);
                        }
                    }

                    ImGui::Separator();
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

    std::vector<Entity> World::getChildren(const Entity& parent, ShouldRecurse recurse) const {
        std::vector<Entity> children;
        std::function<void(const Entity&)> iterateChildren = [&](const Entity& parent) {
            auto it = entityChildren.find(parent);
            if(it != entityChildren.end()) {
                for(auto& entityID : it->second) {
                    Entity child { entityID, const_cast<World&>(*this) };
                    children.emplace_back(child);

                    if(recurse == ShouldRecurse::Recursion) {
                        iterateChildren(child);
                    }
                }
            }
        };

        iterateChildren(parent);
        return children;
    }

    std::optional<Entity> World::getNamedChild(std::string_view name, const Entity& parent, ShouldRecurse recurse) const {
        std::function<std::optional<Entity>(const Entity&)> iterateChildren = [&](const Entity& parent) -> std::optional<Entity> {
            auto it = entityChildren.find(parent);
            if(it != entityChildren.end()) {
                for(auto& entityID : it->second) {
                    Entity child { entityID, const_cast<World&>(*this) };
                    if(child.getName() == name)
                        return child;

                    if(recurse == ShouldRecurse::Recursion) {
                        auto potentialMatch = iterateChildren(child);
                        if(potentialMatch.has_value()) {
                            return potentialMatch;
                        }
                    }
                }
            }
            return {};
        };

        return iterateChildren(parent);
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

    Memory::OptionalRef<Component> World::getComponent(const Entity& entity, ComponentID component) const {
        return getComponent(entity.getID(), component);
    }

    Memory::OptionalRef<Component> World::getComponent(const EntityID& entityID, ComponentID component) const {
        auto componentMapLocation = this->entityComponents.find(entityID);
        if(componentMapLocation == this->entityComponents.end()) {
            // no such entity
            return {};
        }

        auto& componentMap = componentMapLocation->second;
        auto componentLocation = componentMap.find(component);

        if(componentLocation == componentMap.end()) {
            // no such component
            return {};
        }
        return componentLocation->second.get();
    }

    Entity World::wrap(EntityID id) const {
        return Entity(id, const_cast<World&>(*this));
    }

    World& World::operator=(const World& toCopy) {
        entitiesUpdated.clear();
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

    void World::reloadSystems() {
        for(auto& s : logicSystems) {
            s->reload();
        }
        for(auto& s : renderSystems) {
            s->reload();
        }
    }

    void World::unloadSystems() {
        for(auto& s : renderSystems) {
            s->unload();
        }
        for(auto& s : logicSystems) {
            s->unload();
        }
    }

    void World::broadcastStartEvent() {
        for(auto& s : renderSystems) {
            s->broadcastStartEvent();
        }
        for(auto& s : logicSystems) {
            s->broadcastStartEvent();
        }
    }

    void World::broadcastStopEvent() {
        for(auto& s : renderSystems) {
            s->broadcastStopEvent();
        }
        for(auto& s : logicSystems) {
            s->broadcastStopEvent();
        }
    }

    void World::addRenderSystem(std::unique_ptr<System>&& system) {
        verify(system, "System must not be nullptr");
        system->onEntitiesAdded(entities);
        renderSystems.push_back(std::move(system));
    }

    void World::addLogicSystem(std::unique_ptr<System>&& system) {
        verify(system, "System must not be nullptr");
        system->onEntitiesAdded(entities);
        logicSystems.push_back(std::move(system));
    }

    void World::removeLogicSystem(System* system) {
        Carrot::removeIf(logicSystems, [&](auto& ptr) {
            return ptr.get() == system;
        });
    }

    void World::removeRenderSystem(System* system) {
        Carrot::removeIf(renderSystems, [&](auto& ptr) {
            return ptr.get() == system;
        });
    }

    std::vector<System*> World::getLogicSystems() {
        std::vector<System*> result{ logicSystems.size() };
        for(std::size_t i = 0; i < result.size(); i++) {
            result[i] = logicSystems[i].get();
        }
        return result;
    }

    std::vector<System*> World::getRenderSystems() {
        std::vector<System*> result{ renderSystems.size() };
        for(std::size_t i = 0; i < result.size(); i++) {
            result[i] = renderSystems[i].get();
        }
        return result;
    }

    std::vector<const System*> World::getLogicSystems() const {
        std::vector<const System*> result{ logicSystems.size() };
        for(std::size_t i = 0; i < result.size(); i++) {
            result[i] = logicSystems[i].get();
        }
        return result;
    }

    std::vector<const System*> World::getRenderSystems() const {
        std::vector<const System*> result{ renderSystems.size() };
        for(std::size_t i = 0; i < result.size(); i++) {
            result[i] = renderSystems[i].get();
        }
        return result;
    }

    std::optional<Entity> World::findEntityByName(std::string_view name) const {
        for(const auto& [entity, entityName] : entityNames) {
            if(entityName == name) {
                return wrap(entity);
            }
        }
        return {};
    }
}
