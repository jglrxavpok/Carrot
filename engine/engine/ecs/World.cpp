//
// Created by jglrxavpok on 20/02/2021.
//

#include <engine/ecs/components/TransformComponent.h>
#include <engine/math/Transform.h>
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
        getWorld().entityTags[internalEntity] |= tag;
        return *this;
    }

    Tags Entity::getTags() const {
        return getWorld().getTags(*this);
    }

    std::optional<Entity> Entity::getParent() {
        return getWorld().getParent(*this);
    }

    std::optional<const Entity> Entity::getParent() const {
        return getWorld().getParent(*this);
    }

    std::optional<Entity> Entity::getNamedChild(std::string_view name, ShouldRecurse recurse) {
        return getWorld().getNamedChild(name, *this, recurse);
    }

    std::optional<const Entity> Entity::getNamedChild(std::string_view name, ShouldRecurse recurse) const {
        return getWorld().getNamedChild(name, *this, recurse);
    }

    void Entity::setParent(std::optional<Entity> parent) {
        getWorld().setParent(*this, parent);
    }

    void Entity::reparent(std::optional<Entity> parent) {
        getWorld().reparent(*this, parent);
    }

    Carrot::ECS::Entity Entity::duplicate(std::optional<Carrot::ECS::Entity> newParent) {
        return getWorld().duplicate(*this, newParent);
    }

    std::string_view Entity::getName() const {
        return getWorld().getName(*this);
    }

    std::string& Entity::getName() {
        return getWorld().getName(*this);
    }

    void Entity::updateName(std::string_view name) {
        getWorld().entityNames[internalEntity] = name;
    }

    void Entity::remove() {
        getWorld().removeEntity(*this);
    }

    Entity::operator bool() const {
        return exists();
    }

    bool Entity::exists() const {
        return getWorld().exists(internalEntity);
    }

    std::vector<Component*> Entity::getAllComponents() const {
        return getWorld().getAllComponents(*this);
    }

    Memory::OptionalRef<Component> Entity::getComponent(ComponentID component) const {
        return getWorld().getComponent(*this, component);
    }

    Entity& Entity::removeComponent(Component& component) {
        return removeComponent(component.getComponentTypeID());
    }

    Entity& Entity::removeComponent(const ComponentID& componentID) {
        auto& componentMap = getWorld().entityComponents[internalEntity];
        componentMap.erase(componentID);
        getWorld().entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    Signature Entity::getSignature() const {
        return getWorld().getSignature(*this);
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

    void World::invalidateQueries() {
        ZoneScoped;
        // maybe there's a smarter thing to do: add and remove entities from queries directly instead of recomputing query result
        // maybe for the future
        for(const auto& entitySet : { entitiesToAdd, entitiesToRemove }) {
            for(const auto& e : entitySet) {
                const Signature entitySignature = getSignature(wrap(e));

                // if entity matches all components from the query signature, remove the query
                std::erase_if(queries, [&](const QueryResult& query) {
                    if((entitySignature & query.signature) == query.signature) {
                        return true;
                    }
                    return false;
                });
            }
        }

        // entity updates are a bit more involved: the entity signature may no longer match the current signature
        for(const auto& e : entitiesUpdated) {
            std::erase_if(queries, [&](const QueryResult& query) {
                for(const auto& queryEntity : query.matchingEntities) {
                    if(queryEntity.entity.getID() == e) {
                        return true;
                    }
                }
                return false;
            });
        }
    }

    void World::repairLinks(const Carrot::ECS::Entity& root, const std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID>& remapMap) {
        auto remap = [&](const Carrot::ECS::EntityID& id) {
            auto iter = remapMap.find(id);
            if(iter == remapMap.end()) {
                return id;
            }
            return iter->second;
        };

        std::function<void(const Carrot::ECS::Entity&)> recurse = [&](const Carrot::ECS::Entity& e) {
            auto& components = entityComponents[e.getID()];
            for (auto& [componentID, pComponent] : components) {
                pComponent->repairLinks(remap);
            }

            auto& children = entityChildren[e.getID()];
            for(auto& c : children) {
                recurse(wrap(c));
            }
        };

        recurse(root);
    }

    void World::tick(double dt) {
        for(const auto& toAdd : entitiesToAdd) {
            entities.push_back(toAdd);
        }
        invalidateQueries();
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

        worldData.update();
    }

    void World::prePhysics() {
        if(!frozenLogic) {
            for(const auto& logic : logicSystems) {
                logic->prePhysics();
            }
        }

        for(const auto& render : renderSystems) {
            render->prePhysics();
        }
    }

    void World::postPhysics() {
        if(!frozenLogic) {
            for(const auto& logic : logicSystems) {
                logic->postPhysics();
            }
        }

        for(const auto& render : renderSystems) {
            render->postPhysics();
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

    void World::reparent(Entity& toSet, std::optional<Entity> newParent) {
        assert(toSet);
        auto oldParent = toSet.getParent();

        // special handling for transform component
        Carrot::Math::Transform globalTransform;
        auto transformComp = toSet.getComponent<Carrot::ECS::TransformComponent>();
        if(transformComp.hasValue()) {
            globalTransform = transformComp->computeGlobalPhysicsTransform();
            globalTransform.scale = transformComp->computeFinalScale();
        }

        toSet.setParent(newParent);

        // special handling for transform component
        if(transformComp.hasValue()) {
            transformComp->setGlobalTransform(globalTransform);
        }
    }

    Carrot::ECS::Entity World::duplicate(const Carrot::ECS::Entity& entity, std::optional<Carrot::ECS::Entity> newParent) {
        std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID> remap;
        std::function<Carrot::ECS::Entity(const Carrot::ECS::Entity&, std::optional<Carrot::ECS::Entity> newParent)> duplicateEntity =
            [&](const Carrot::ECS::Entity& toClone, std::optional<Carrot::ECS::Entity> newParent) {
                auto clone = newEntity(std::string(toClone.getName())+" (Copy)");
                remap[toClone.getID()] = clone.getID();
                for(const auto* comp : toClone.getAllComponents()) {
                    clone.addComponent(std::move(comp->duplicate(clone)));
                }

                for(const auto& child : getChildren(toClone)) {
                    duplicateEntity(child, clone);
                }

                if(newParent) {
                    clone.setParent(*newParent);
                } else {
                    clone.setParent(entity.getParent());
                }
                return clone;
            };

        auto clone = duplicateEntity(entity, std::move(newParent));

        repairLinks(clone, remap);
        return clone;
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

    std::span<const EntityWithComponents> World::queryEntities(const std::unordered_set<Carrot::ComponentID>& componentIDs) {
        Signature signature;
        for(const auto& compID : componentIDs) {
            signature.addComponent(compID);
        }

        return queryEntities(signature);
    }

    std::span<const EntityWithComponents> World::queryEntities(const Signature& signature) {
        for(auto& query : queries) {
            if(query.signature == signature) {
                return query.matchingEntities;
            }
        }

        QueryResult& newQuery = queries.emplace_back();
        newQuery.signature = signature;
        std::vector<Entity> result;
        for(const auto& entityID : entities) {
            auto entity = wrap(entityID);
            if((getSignature(entity) & signature) == signature) {
                result.push_back(entity);
            }
        }
        newQuery.matchingEntities.resize(result.size());

        fillComponents(newQuery.signature, result, newQuery.matchingEntities);
        return newQuery.matchingEntities;
    }

    void World::fillComponents(const Signature& signature, std::span<const Entity> _entities, std::span<EntityWithComponents> entitiesWithComponents) {
        verify(_entities.size() == entitiesWithComponents.size(), "entities.size() != entitiesWithComponents.size()");
        std::size_t componentCount = signature.getComponentCount();

        for(std::size_t componentID = 0; componentID < MAX_COMPONENTS; componentID++) {
            if(signature.hasComponent(componentID)) {
                Signature::IndexType componentIndex = signature.getComponentIndex(componentID);

                for(std::size_t i = 0; i < _entities.size(); i++) {
                    auto& withComponents = entitiesWithComponents[i];
                    const auto& entity = _entities[i];
                    withComponents.entity = entity;
                    withComponents.components.resize(componentCount);

                    Memory::OptionalRef<Component> component = entity.getComponent(componentID);
                    verify(component.hasValue(), "Component is not in entity??");
                    withComponents.components[componentIndex] = component.asPtr();
                }
            }
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
        entityComponents.clear();

        for(const auto& [entityID, componentMap] : toCopy.entityComponents) {
            auto& destComponents = entityComponents[entityID];
            for(const auto& [id, comp] : componentMap) {
                destComponents[id] = comp->duplicate(wrap(entityID));
            }
        }

        auto copySystems = [&](std::vector<std::unique_ptr<System>>& dest, const std::vector<std::unique_ptr<System>>& src) {
            dest.clear();
            dest.resize(src.size());

            for (std::size_t i = 0; i < src.size(); ++i) {
                dest[i] = src[i]->duplicate(*this);
                for(const auto& srcEntity : src[i]->entities) {
                    dest[i]->entities.emplace_back(srcEntity.internalEntity, *this);
                }
                dest[i]->onEntitiesUpdated({});
            }
        };
        copySystems(logicSystems, toCopy.logicSystems);
        copySystems(renderSystems, toCopy.renderSystems);

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

    WorldData& World::getWorldData() {
        return worldData;
    }

    const WorldData& World::getWorldData() const {
        return worldData;
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

    void World::reloadSystemEntities(System* system) {
        system->onEntitiesUpdated(entities);
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
