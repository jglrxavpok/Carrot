//
// Created by jglrxavpok on 31/05/2023.
//

#pragma once

#include <commands/UndoStack.h>
#include <commands/UpdateComponentCommands.h>
#include <Peeler.h>
#include <panels/InspectorPanel.h>
#include <core/utils/ImGuiUtils.hpp>
#include <engine/ecs/Prefab.h>
#include <engine/edition/DragDropTypes.h>
#include <engine/Engine.h>

namespace Carrot::ECS {
    class CSharpComponent;
}

namespace Carrot::Render {
    using bool32 = uint32_t;
}

namespace Peeler {
    namespace Helpers {
        /**
         * Wrapper of angles to use a different slider than "regular" floats
         */
        struct AngleWrapper {
            float radianValue;

            bool operator==(const AngleWrapper &) const = default;
            bool operator!=(const AngleWrapper &) const = default;
        };

        /**
         * Wrapper of angles to use a different slider than "regular" floats, but described as their cos value
         */
        struct CosAngleWrapper {
            float cosRadianValue;

            bool operator==(const CosAngleWrapper &) const = default;
            bool operator!=(const CosAngleWrapper &) const = default;
        };

        /**
         * Wrapper for vec3 used as RGB color values
         */
        struct RGBColorWrapper {
            glm::vec3 rgb;

            bool operator==(const RGBColorWrapper &) const = default;
            bool operator!=(const RGBColorWrapper &) const = default;
        };

        /**
         * Wrapper for vec3 used as RGB color values
         */
        struct RGBAColorWrapper {
            glm::vec4 rgba;

            bool operator==(const RGBAColorWrapper &) const = default;
            bool operator!=(const RGBAColorWrapper &) const = default;
        };

        static bool any(std::span<bool> values) {
            for(const bool& b : values) {
                if(b) {
                    return true;
                }
            }
            return false;
        }

        static bool all(std::span<bool> values) {
            for(const bool& b : values) {
                if(!b) {
                    return false;
                }
            }
            return true;
        }

        template<typename T>
        struct Limits {};

        template<>
        struct Limits<float> {
            float min = FLT_MIN;
            float max = FLT_MAX;

            const char* formatOverride = nullptr;
        };

        template<>
        struct Limits<std::int32_t> {
            std::int32_t min = std::numeric_limits<std::int32_t>::min();
            std::int32_t max = std::numeric_limits<std::int32_t>::max();
        };

        template<>
        struct Limits<AngleWrapper> {
            float min = -2 * glm::pi<float>();
            float max = 2 * glm::pi<float>();
        };

        template<>
        struct Limits<std::string_view> {
            bool multiline = false;
        };

        template<>
        struct Limits<std::string> {
            bool multiline = false;
        };

        template<>
        struct Limits<Carrot::IO::VFS::Path> {
            bool allowDirectories = false;
            std::function<bool(const Carrot::IO::VFS::Path&)> validityChecker = [](const Carrot::IO::VFS::Path& p) { return true; };
        };
    }

    template<typename T>
    static bool editMultiple(const char* id, std::span<T> values, const Helpers::Limits<T>& limits = {}) = delete;

    // instead of using ImGui::DragFloat3 & co, reimplement it. Because multiple positions are modified at once,
    //  they might not have all the same coordinates. This way, we can modify only the coordinate that has been modified
    template<int ComponentCount>
    static bool editMultipleVec(const char* id, std::span<glm::vec<ComponentCount, float>> vectors, const Helpers::Limits<glm::vec<ComponentCount, float>>& limits) {
        using VecType = glm::vec<ComponentCount, float>;
        bool modified = false;
        ImGui::BeginGroup();
        ImGui::PushID(id);

        ImGui::PushMultiItemsWidths(ComponentCount, ImGui::CalcItemWidth());

        for(std::size_t axisIndex = 0; axisIndex < ComponentCount; axisIndex++) {
            ImGui::PushID(axisIndex);
            float currentValue = (vectors[0])[axisIndex];
            bool sameValue = true;

            for(std::size_t j = 1; j < vectors.size(); j++) {
                if(currentValue != (vectors[j])[axisIndex]) {
                    sameValue = false;
                }
            }

            const char* format = sameValue ? nullptr : "<VARIOUS>";

            if (axisIndex != 0) {
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            }

            if(ImGui::DragScalar("", ImGuiDataType_Float, &currentValue, 1, nullptr, nullptr, format)) {
                modified = true;
                for(VecType& vec : vectors) {
                    vec[axisIndex] = currentValue;
                }
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(id);

        ImGui::PopID();
        ImGui::EndGroup();
        return modified;
    }

    template<>
    inline bool editMultiple<glm::vec2>(const char* id, std::span<glm::vec2> vectors, const Helpers::Limits<glm::vec2>& limits) {
        return editMultipleVec<2>(id, vectors, limits);
    }

    template<>
    inline bool editMultiple<glm::vec3>(const char* id, std::span<glm::vec3> vectors, const Helpers::Limits<glm::vec3>& limits) {
        return editMultipleVec<3>(id, vectors, limits);
    }

    template<>
    inline bool editMultiple<glm::vec4>(const char* id, std::span<glm::vec4> vectors, const Helpers::Limits<glm::vec4>& limits) {
        return editMultipleVec<4>(id, vectors, limits);
    }

    // maybe merge with function editMultipleVec3, but for now copy paste will do
    template<>
    inline bool editMultiple<glm::quat>(const char* id, std::span<glm::quat> values, const Helpers::Limits<glm::quat>& limits) {
        bool modified = false;
        ImGui::BeginGroup();
        ImGui::PushID(id);

        ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());

        for(std::size_t axisIndex = 0; axisIndex < 4; axisIndex++) {
            ImGui::PushID(axisIndex);
            float currentValue = (values[0])[axisIndex];
            bool sameValue = true;

            for(std::size_t j = 1; j < values.size(); j++) {
                if(currentValue != (values[j])[axisIndex]) {
                    sameValue = false;
                }
            }

            const char* format = sameValue ? nullptr : "<VARIOUS>";

            if (axisIndex != 0) {
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            }

            if(ImGui::DragScalar("", ImGuiDataType_Float, &currentValue, 1, nullptr, nullptr, format)) {
                modified = true;
                for(glm::quat& q : values) {
                    q[axisIndex] = currentValue;
                }
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(id);

        ImGui::PopID();
        ImGui::EndGroup();
        return modified;
    }

    template<>
    inline bool editMultiple<bool>(const char* id, std::span<bool> values, const Helpers::Limits<bool>& limits) {
        int tristate = -1;
        if(Helpers::all(values)) {
            tristate = 1;
        } else if(!Helpers::any(values)) {
            tristate = 0;
        }
        if(ImGui::CheckBoxTristate(id, &tristate)) {
            for(bool& b : values) {
                b = tristate == 1;
            }
            return true;
        }
        return false;
    }

    template<>
    inline bool editMultiple<Carrot::Render::bool32>(const char* id, std::span<Carrot::Render::bool32> values, const Helpers::Limits<Carrot::Render::bool32>& limits) {
        Carrot::Vector<bool> asBools;
        asBools.resize(values.size());
        for(std::int64_t i = 0; i < asBools.size(); i++) {
            asBools[i] = values[i];
        }
        if(editMultiple(id, std::span<bool>(asBools))) {
            for(auto& v : values) {
                v = asBools[0];
            }
            return true;
        }
        return false;
    }

    template<>
    inline bool editMultiple<float>(const char* id, std::span<float> values, const Helpers::Limits<float>& limits) {
        float sameValue = values[0];
        bool areSame = true;
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i] != sameValue) {
                areSame = false;
                break;
            }
        }

        const char* format = areSame ? (limits.formatOverride ? limits.formatOverride : "%.3f") : "<VARIOUS>";

        bool changed = false;
        if(limits.min != FLT_MIN && limits.max != FLT_MAX) {
            float min = std::min(limits.min, limits.max);
            float max = std::max(limits.min, limits.max);

            // ImGui limits
            bool useSlider = true;
            if(min < -FLT_MAX) {
                min = -FLT_MAX;
                useSlider = false;
            }
            if(max > FLT_MAX) {
                max = FLT_MAX;
                useSlider = false;
            }

            if(useSlider) {
                changed |= ImGui::SliderFloat(id, &sameValue, min, max, format);
            } else {
                changed |= ImGui::DragFloat(id, &sameValue, 0.1f/*TODO: speed attribute*/, min, max, format);
            }
        } else {
            changed |= ImGui::DragFloat(id, &sameValue, 0.1f/*TODO: speed attribute*/, 0.0f, 0.0f, format);
        }
        if(changed) {
            for(float& v : values) {
                v = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    inline bool editMultiple<std::int32_t>(const char* id, std::span<std::int32_t> values, const Helpers::Limits<std::int32_t>& limits) {
        int sameValue = values[0]; // ImGui uses int
        bool areSame = true;
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i] != sameValue) {
                areSame = false;
                break;
            }
        }

        const char* format = areSame ? "%d" : "<VARIOUS>";

        bool changed = false;
        if(limits.min != std::numeric_limits<std::int32_t>::min() && limits.max != std::numeric_limits<std::int32_t>::max()) {
            std::int32_t min = std::min(limits.min, limits.max);
            std::int32_t max = std::max(limits.min, limits.max);

            // ImGui limits
            bool useSlider = true;
            if(min < std::numeric_limits<std::int32_t>::min()) {
                min = std::numeric_limits<std::int32_t>::min();
                useSlider = false;
            }
            if(max > std::numeric_limits<std::int32_t>::max()) {
                max = std::numeric_limits<std::int32_t>::max();
                useSlider = false;
            }

            if(useSlider) {
                changed |= ImGui::SliderInt(id, &sameValue, min, max, format);
            } else {
                changed |= ImGui::DragInt(id, &sameValue, 1/*TODO: speed attribute*/, min, max, format);
            }
        } else {
            changed |= ImGui::DragInt(id, &sameValue, 1/*TODO: speed attribute*/, 0.0f, 0.0f, format);
        }
        if(changed) {
            for(std::int32_t& v : values) {
                v = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    inline bool editMultiple<Helpers::AngleWrapper>(const char* id, std::span<Helpers::AngleWrapper> values, const Helpers::Limits<Helpers::AngleWrapper>& limits) {
        float sameValue = values[0].radianValue;
        bool areSame = true;
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i].radianValue != sameValue) {
                areSame = false;
                break;
            }
        }

        const char* format = areSame ? "%.0f deg" : "<VARIOUS>";
        if(ImGui::SliderAngle(id, &sameValue, Carrot::Math::Rad2Degrees * limits.min, Carrot::Math::Rad2Degrees * limits.max, format)) {
            for(Helpers::AngleWrapper& angle : values) {
                angle.radianValue = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    inline bool editMultiple<Helpers::CosAngleWrapper>(const char* id, std::span<Helpers::CosAngleWrapper> values, const Helpers::Limits<Helpers::CosAngleWrapper>& limits) {
        Carrot::Vector<Helpers::AngleWrapper> asAngles;
        asAngles.resize(values.size());
        for(std::int64_t i = 0; i < asAngles.size(); i++) {
            asAngles[i].radianValue = glm::acos(values[i].cosRadianValue);
        }
        if(editMultiple(id, std::span<Helpers::AngleWrapper>(asAngles))) {
            for(auto& v : values) {
                v.cosRadianValue = glm::cos(asAngles[0].radianValue);
            }
            return true;
        }
        return false;
    }

    template<>
    inline bool editMultiple<Helpers::RGBColorWrapper>(const char* id, std::span<Helpers::RGBColorWrapper> values, const Helpers::Limits<Helpers::RGBColorWrapper>& limits) {
        const glm::vec3& colorValue = values[0].rgb;
        float colorArr[3] { colorValue.r, colorValue.g, colorValue.b };
        if(ImGui::ColorPicker3(id, colorArr)) {
            for(Helpers::RGBColorWrapper& v : values) {
                v.rgb = glm::vec3 { colorArr[0], colorArr[1], colorArr[2] };
            }
            return true;
        }
        return false;
    }

    template<>
    inline bool editMultiple<Helpers::RGBAColorWrapper>(const char* id, std::span<Helpers::RGBAColorWrapper> values, const Helpers::Limits<Helpers::RGBAColorWrapper>& limits) {
        const glm::vec4& colorValue = values[0].rgba;
        float colorArr[4] { colorValue.r, colorValue.g, colorValue.b, colorValue.a };
        if(ImGui::ColorPicker4(id, colorArr)) {
            for(Helpers::RGBAColorWrapper& v : values) {
                v.rgba = glm::vec4 { colorArr[0], colorArr[1], colorArr[2], colorArr[3] };
            }
            return true;
        }
        return false;
    }

    template<>
    inline bool editMultiple<std::string_view>(const char* id, std::span<std::string_view> values, const Helpers::Limits<std::string_view>& limits) {
        static std::string sameValue; // expected to have its contents copied immediatly after returning out of this function
        sameValue = std::string { values[0] };
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i] != sameValue) {
                sameValue = "<VARIOUS>";
                break;
            }
        }

        bool modified = false;
        if(limits.multiline) {
            modified = ImGui::InputTextMultiline(id, sameValue);
        } else {
            modified = ImGui::InputText(id, sameValue);
        }

        if(modified) {
            for(std::string_view& v : values) {
                v = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    inline bool editMultiple<std::string>(const char* id, std::span<std::string> values, const Helpers::Limits<std::string>& limits) {
        std::string sameValue = std::string { values[0] };
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i] != sameValue) {
                sameValue = "<VARIOUS>";
                break;
            }
        }

        bool modified = false;
        if(limits.multiline) {
            modified = ImGui::InputTextMultiline(id, sameValue);
        } else {
            modified = ImGui::InputText(id, sameValue);
        }

        if(modified) {
            for(std::string& v : values) {
                v = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    inline bool editMultiple<Carrot::ECS::Entity>(const char* id, std::span<Carrot::ECS::Entity> values, const Helpers::Limits<Carrot::ECS::Entity>& limits) {
        Carrot::ECS::Entity sameValue { values[0] };
        const char* nameOverride = nullptr;
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i].getID() != sameValue.getID()) {
                nameOverride = "<VARIOUS>";
                break;
            }
        }

        if(Peeler::Instance->drawPickEntityWidget(id, sameValue, nameOverride)) {
            for(Carrot::ECS::Entity& v : values) {
                v = sameValue;
            }
            return true;
        }
        return false;
    }

    bool editVFSPath(const char* id, Carrot::IO::VFS::Path& path, const Helpers::Limits<Carrot::IO::VFS::Path>& limits = {});

    template<>
    inline bool editMultiple<Carrot::IO::VFS::Path>(const char* id, std::span<Carrot::IO::VFS::Path> values, const Helpers::Limits<Carrot::IO::VFS::Path>& limits) {
        std::string pathStr = values[0].toString();

        for(const auto& v : values) {
            if(pathStr != v.toString()) {
                pathStr = "<VARIOUS>";
                break;
            }
        }

        Carrot::IO::VFS::Path tmp { pathStr };
        bool wasModified = editVFSPath(id, tmp, limits);

        if(wasModified) {
            for(auto& v : values) {
                v = tmp;
            }
        }
        return wasModified;
    }

    template<>
    inline bool editMultiple<Carrot::Render::Texture::Ref>(const char* id, std::span<Carrot::Render::Texture::Ref> values, const Helpers::Limits<Carrot::Render::Texture::Ref>& limits) {
        Carrot::Vector<Carrot::IO::VFS::Path> paths;
        paths.ensureReserve(values.size());
        for(const auto& texture : values) {
            if(texture) {
                paths.emplaceBack(texture->getOriginatingResource().getName());
            } else {
                paths.emplaceBack();
            }
        }
        bool modified = editMultiple<Carrot::IO::VFS::Path>(id, paths,
            Helpers::Limits<Carrot::IO::VFS::Path> {
                .validityChecker = [](const Carrot::IO::VFS::Path& path) {
                    return Carrot::IO::isImageFormat(path.toString().c_str());
                }
            });
        if(modified) {
            Carrot::Render::Texture::Ref texture = GetAssetServer().blockingLoadTexture(paths[0]);
            for(auto& v : values) {
                v = texture;
            }
        }
        return modified;
    }

    /**
     * Helper function for editing the same property in multiple entities at once. Handles undo stack automatically
     * @tparam TComponent Type of component to edit
     * @tparam TPropertyType Type of property to edit (underlying type, for positions this would be vec3)
     * @param id label used to show user which property they are editing
     * @param components list of components to edit
     * @param getterFunc function to get the property's value
     * @param setterFunc function to set the property's value
     */
    template<typename TComponent, typename TPropertyType>
    static void multiEditField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*> components,
        std::function<TPropertyType(TComponent&)> getterFunc,
        std::function<void(TComponent&, const TPropertyType&)> setterFunc,
        const Helpers::Limits<TPropertyType>& limits = {}
    ) {
        Carrot::Vector<TPropertyType> values;
        values.ensureReserve(components.size());
        for(TComponent* pComponent : components) {
            values.emplaceBack(getterFunc(*pComponent));
        }

        bool modified = editMultiple<TPropertyType>(id, values, limits);

        if(modified) {
            verify(edition.editor.inspectorType == Application::InspectorType::Entities, "Code assumes we are editing an entity");
            if(edition.editor.inspectorType == Application::InspectorType::Entities) {
                edition.editor.undoStack.push<UpdateComponentValues<TComponent, TPropertyType>>(Carrot::sprintf("Update %s", id), edition.editor.selectedEntityIDs, values, getterFunc, setterFunc, components[0]->getComponentTypeID());
            }
            edition.hasModifications = true;
        }
    }

    /**
     * Helper function for editing the same property in multiple entities at once. Handles undo stack automatically
     * @tparam TComponent Type of component to edit
     * @tparam TPropertyType Type of property to edit (underlying type, for positions this would be vec3)
     * @param id label used to show user which property they are editing
     * @param components list of components to edit
     * @param getter function to get the property's value
     * @param setter function to set the property's value
     */
    template<typename TComponent, typename TPropertyType>
    static void multiEditField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*> components,
        TPropertyType(getter)(TComponent&),
        void(setter)(TComponent&, const TPropertyType&),
        const Helpers::Limits<TPropertyType>& limits = {}
    ) {
        multiEditField(edition, id, components, std::function(getter), std::function(setter), limits);
    }

    /**
     * Helper function for editing the same property in multiple entities at once. Handles undo stack automatically
     * @tparam TComponent Type of component to edit
     * @tparam TPropertyType Type of property to edit (underlying type, for positions this would be vec3)
     * @param id label used to show user which property they are editing
     * @param components list of components to edit
     * @param accessor function to access the property's value
     */
    template<typename TComponent, typename TPropertyType>
    static void multiEditField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*> components, TPropertyType&(accessor)(TComponent&), const Helpers::Limits<TPropertyType>& limits = {}) {
        multiEditField(edition, id, components,
            std::function([&](TComponent& c) -> TPropertyType { return accessor(c); }),
            std::function([&](TComponent& c, const TPropertyType& v) { accessor(c) = v; }),
            limits);
    }

    template<typename TComponent, typename TEnum>
    static void multiEditEnumField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*>& components,
        std::function<TEnum(TComponent&)> getter,
        std::function<void(TComponent&, const TEnum&)> setter,
        const char*(nameGetter)(const TEnum&),
        const Carrot::Vector<TEnum>& validValues,
        const Helpers::Limits<TEnum>& limits = {})
    {
        Carrot::Vector<TEnum> values;
        values.ensureReserve(components.size());
        TEnum currentlySelectedValue = getter(*components[0]);
        bool allSame = true;
        for(TComponent* pComponent : components) {
            values.emplaceBack(getter(*pComponent));
            allSame = currentlySelectedValue == values[values.size()-1];
        }

        bool modified = false;
        const char* previewValue = allSame ? nameGetter(currentlySelectedValue) : "<VARIOUS>";
        if(ImGui::BeginCombo(id, previewValue)) {
            auto selectable = [&](TEnum entry) {
                bool selected = entry == currentlySelectedValue;
                if(ImGui::Selectable(nameGetter(entry), &selected)) {
                    modified = true;
                    for(auto& v : values) {
                        v = entry;
                    }
                }
            };

            for(const auto& validValue : validValues) {
                selectable(validValue);
            }

            ImGui::EndCombo();
        }

        if(modified) {
            verify(edition.editor.inspectorType == Application::InspectorType::Entities, "Code assumes we are editing an entity");
            if(edition.editor.inspectorType == Application::InspectorType::Entities) {
                edition.editor.undoStack.push<UpdateComponentValues<TComponent, TEnum>>(Carrot::sprintf("Update %s", id), edition.editor.selectedEntityIDs, values, getter, setter, components[0]->getComponentTypeID());
            }
        }
    }

    template<typename TComponent, typename TEnum>
    static void multiEditEnumField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*>& components, TEnum(getter)(TComponent&), void(setter)(TComponent&, const TEnum&),
        const char*(nameGetter)(const TEnum&),
        const Carrot::Vector<TEnum>& validValues,
        const Helpers::Limits<TEnum>& limits = {}) {
        multiEditEnumField<TComponent, TEnum>(edition, id, components, std::function(getter), std::function(setter), nameGetter, validValues, limits);
    }
}

namespace Peeler {
    /**
     * Register all edition functions below to the given inspector
     */
    void registerEditionFunctions(InspectorPanel& inspector);
    void registerDisplayNames(InspectorPanel& inspector);

    void editCSharpComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components);

    namespace Helpers {
        /**
         * Checks whether all values in 'values' are true
         */
        bool all(std::span<bool> values);

        /**
         * Checks whether any value in 'values' is true
         */
        bool any(std::span<bool> values);
    }
}