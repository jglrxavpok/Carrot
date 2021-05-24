//
// Created by jglrxavpok on 23/05/2021.
//

#pragma once
#include <cstdint>

namespace Carrot {

    struct ExpressionType {
    public:
        explicit ExpressionType(std::string name): internalID(ids++), typeName(std::move(name)) {};

        bool operator==(const ExpressionType& other) const {
            return internalID == other.internalID;
        };

        std::string name() const {
            return typeName;
        }

    private:
        static std::uint32_t ids;
        std::uint32_t internalID = 0;
        std::string typeName;
    };

    namespace ExpressionTypes {
        inline ExpressionType Void{"Void"};
        inline ExpressionType Int{"Int"};
        inline ExpressionType Float{"Float"};
        inline ExpressionType Bool{"Bool"};
    };
}