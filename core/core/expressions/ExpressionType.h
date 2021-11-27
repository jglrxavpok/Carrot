//
// Created by jglrxavpok on 23/05/2021.
//

#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Carrot {

    struct ExpressionType {
    public:
        explicit ExpressionType(std::string name): internalID(ids++), typeName(std::move(name)) {};

        bool operator==(const ExpressionType& other) const {
            return internalID == other.internalID;
        };

        auto operator<=>(const ExpressionType& other) const {
            return internalID <=> other.internalID;
        };

        std::string name() const {
            return typeName;
        }

    private:
        static std::atomic<std::uint32_t> ids;
        std::uint32_t internalID = 0;
        std::string typeName;

        friend struct std::hash<Carrot::ExpressionType>;
    };

    namespace ExpressionTypes {
        inline ExpressionType Void{"void"};
        inline ExpressionType Int{"int"};
        inline ExpressionType Float{"float"};
        inline ExpressionType Bool{"bool"};

        inline ExpressionType fromName(const std::string& name) {
            if(name == "void")
                return Void;

            if(name == "int")
                return Int;

            if(name == "float")
                return Float;

            if(name == "bool")
                return Bool;

            throw std::runtime_error("Unknown expression type name: " + name);
        }
    };
}

namespace std {
    template <>
    struct hash<Carrot::ExpressionType>
    {
        std::size_t operator()(const Carrot::ExpressionType& k) const
        {
            using std::size_t;
            using std::hash;
            using std::string;

            // Compute individual hash values for first,
            // second and third and combine them using XOR
            // and bit shifting:

            return hash<std::uint32_t>()(k.internalID);
        }
    };
}