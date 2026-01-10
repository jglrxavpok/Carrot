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

        UpdateComponentValues(Application &app, const std::string& desc, std::unordered_set<Carrot::ECS::EntityID> _entityList, std::span<TValue> _newValues,
            std::function<TValue(TComponent& comp)> _getter, std::function<void(TComponent& comp, const TValue& value)> _setter,
            Carrot::ComponentID _componentID)
            : ICommand(app, desc)
            , newValues(_newValues)
            , getter(_getter)
            , setter(_setter)
            , componentID(_componentID)
        {
            entityList.ensureReserve(_entityList.size());
            for (auto& ent : _entityList) {
                entityList.emplaceBack() = ent;
            }
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
     * Command which updates the value of a system property.
     * Name has to be explicitely specified
     */
    template<typename TSystem, typename TValue>
    struct UpdateSystemValue: Peeler::ICommand {
        std::string systemName;
        bool isRenderSystem = false;
        TValue newValue;
        TValue oldValue;
        std::function<TValue(TSystem& comp)> getter;
        std::function<void(TSystem& comp, const TValue& value)> setter;

        UpdateSystemValue(Application &app, const std::string& desc, const std::string& _systemName, bool isRenderSystem, const TValue& _newValue,
            std::function<TValue(TSystem& comp)> _getter, std::function<void(TSystem& comp, const TValue& value)> _setter)
            : ICommand(app, desc)
            , isRenderSystem(isRenderSystem)
            , systemName(_systemName)
            , newValue(_newValue)
            , getter(_getter)
            , setter(_setter)
        {
            oldValue = getter(reinterpret_cast<TSystem&>(*getSystem()));
        }

        Carrot::ECS::System* getSystem() {
            Carrot::ECS::System* pSystem = nullptr;
            if (isRenderSystem) {
                pSystem = editor.currentScene.world.getRenderSystem(systemName);
            } else {
                pSystem = editor.currentScene.world.getLogicSystem(systemName);
            }
            verify(pSystem, Carrot::sprintf("No system matching with name %s in current scene", systemName.c_str()));
            return pSystem;
        }

        void undo() override {
            setter(reinterpret_cast<TSystem&>(*getSystem()), oldValue);
        }

        void redo() override {
            setter(reinterpret_cast<TSystem&>(*getSystem()), newValue);
        }
    };
}