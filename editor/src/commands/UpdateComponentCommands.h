//
// Created by jglrxavpok on 08/05/2024.
//

#pragma once

#include <Peeler.h>
#include <commands/UndoStack.h>
#include <engine/ecs/EntityTypes.h>
#include <engine/ecs/Prefab.h>

namespace Peeler {
    /**
     * Command which updates the value of a component property.
     * Component ID has to be explicitely specified (for support of C# components).
     */
    template<typename TComponent, typename TValue>
    struct UpdateComponentValues: Peeler::ICommand {
        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<TValue> newValues;
        Carrot::Vector<TValue> oldValues;
        std::function<TValue(TComponent& comp)> getter;
        std::function<void(TComponent& comp, const TValue& value)> setter;

        Carrot::ComponentID componentID;

        UpdateComponentValues(Application &app, const std::string& desc, std::span<Carrot::ECS::EntityID> _entityList, std::span<TValue> _newValues,
            std::function<TValue(TComponent& comp)> _getter, std::function<void(TComponent& comp, const TValue& value)> _setter,
            Carrot::ComponentID _componentID)
            : ICommand(app, desc)
            , entityList(_entityList)
            , newValues(_newValues)
            , getter(_getter)
            , setter(_setter)
            , componentID(_componentID)
        {
            oldValues.ensureReserve(newValues.size());
            for(const auto& entityID : _entityList) {
                oldValues.pushBack(getter(reinterpret_cast<TComponent&>(app.currentScene.world.getComponent(entityID, componentID).asRef())));
            }
        }

        void undo() override {
            for(std::size_t i = 0; i < entityList.size(); i++) {
                setter(reinterpret_cast<TComponent&>(editor.currentScene.world.getComponent(entityList[i], componentID).asRef()), oldValues[i]);
            }
        }

        void redo() override {
            for(std::size_t i = 0; i < entityList.size(); i++) {
                setter(reinterpret_cast<TComponent&>(editor.currentScene.world.getComponent(entityList[i], componentID).asRef()), newValues[i]);
            }
        }
    };

    /**
     * Command which updates the value of a component property, inside a Prefab.
     * Component ID has to be explicitely specified (for support of C# components).
     */
    template<typename TComponent, typename TValue>
    struct UpdatePrefabComponentValues: Peeler::ICommand {
        Carrot::IO::VFS::Path prefabPath;
        TValue newValue;
        TValue oldValue;
        std::function<TValue(TComponent& comp)> getter;
        std::function<void(TComponent& comp, const TValue& value)> setter;

        Carrot::ComponentID componentID;

        std::shared_ptr<Carrot::ECS::Prefab> getPrefab() {
            auto pPrefab = GetAssetServer().blockingLoadPrefab(prefabPath);
            verify(pPrefab, Carrot::sprintf("No prefab at path '%s'", prefabPath.toString().c_str()));
            return pPrefab;
        }

        UpdatePrefabComponentValues(Application &app, const std::string& desc, const Carrot::IO::VFS::Path& _prefabPath, std::span<TValue> _newValues,
            std::function<TValue(TComponent& comp)> _getter, std::function<void(TComponent& comp, const TValue& value)> _setter,
            Carrot::ComponentID _componentID)
            : ICommand(app, desc)
            , prefabPath(_prefabPath)
            , newValue(_newValues[0])
            , getter(_getter)
            , setter(_setter)
            , componentID(_componentID)
        {
            oldValue = getter(reinterpret_cast<TComponent&>(getPrefab()->getComponent(componentID).asRef()));
        }

        void undo() override {
            auto pPrefab = getPrefab();
            setter(reinterpret_cast<TComponent&>(pPrefab->getComponent(componentID).asRef()), oldValue);
            pPrefab->applyComponentChangeToInstances<TComponent, TValue>(editor.currentScene.world, componentID, getter, setter, newValue, oldValue);
        }

        void redo() override {
            auto pPrefab = getPrefab();
            setter(reinterpret_cast<TComponent&>(pPrefab->getComponent(componentID).asRef()), newValue);
            pPrefab->applyComponentChangeToInstances<TComponent, TValue>(editor.currentScene.world, componentID, getter, setter, oldValue, newValue);
        }
    };
}