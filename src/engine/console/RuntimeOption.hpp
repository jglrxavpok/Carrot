//
// Created by jglrxavpok on 02/06/2021.
//

#pragma once
#include <string>
#include <vector>

namespace Carrot {

    /// Option that can be turned on and off during runtime via the console. Intented to be kept in static storage so that the console
    /// can access all runtime options at once
    class RuntimeOption {
    public:
        explicit RuntimeOption(std::string name, bool defaultValue): name(std::move(name)), currentValue(defaultValue) {
            getAllOptions().push_back(this);
        }

        operator bool() const {
            return currentValue;
        }

        bool& getValueRef() {
            return currentValue;
        }

        void setValue(bool newState) {
            currentValue = newState;
        }

        [[nodiscard]] const std::string& getName() const {
            return name;
        }

        static std::vector<RuntimeOption*>& getAllOptions() {
            static std::vector<RuntimeOption*> allOptions;
            return allOptions;
        }
    private:
        bool currentValue;
        std::string name;
    };

}