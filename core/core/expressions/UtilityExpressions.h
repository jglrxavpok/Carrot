//
// Created by jglrxavpok on 10/06/2021.
//

#pragma once

#include "core/utils/UUID.h"
#include "core/expressions/Expression.h"
#include <cassert>

namespace Carrot {
    class OnceExpression: public Expression {
    public:
        inline explicit OnceExpression(Carrot::UUID id, std::shared_ptr<Carrot::Expression> toExecute): Expression(), id(id), toExecute(toExecute) {
            assert(toExecute->getType() == Carrot::ExpressionTypes::Void);
        }

        ExpressionType getType() override { return Carrot::ExpressionTypes::Void; }
        const Carrot::UUID& getUUID() const { return id; }

        inline std::string toString() const override {
            return "Once(" + id.toString()+ ", " + toExecute->toString()+")";
        }

        inline std::shared_ptr<Carrot::Expression> getExpressionToExecute() const { return toExecute; }

        std::any visit(BaseExpressionVisitor& visitor) override;

    private:
        Carrot::UUID id;
        std::shared_ptr<Carrot::Expression> toExecute;
    };

    class PrefixedExpression: public Expression {
    private:
        std::shared_ptr<Carrot::Expression> prefix;
        std::shared_ptr<Carrot::Expression> expression;

    public:
        inline explicit PrefixedExpression(std::shared_ptr<Carrot::Expression> prefix, std::shared_ptr<Carrot::Expression> expression): Expression(), prefix(prefix), expression(expression) {
            assert(prefix->getType() == Carrot::ExpressionTypes::Void);
        }

        ExpressionType getType() override { return expression->getType(); }

        inline std::string toString() const override {
            return "Prefixed(" + prefix->toString() + ", " + expression->toString()+")";
        }

        inline std::shared_ptr<Carrot::Expression> getExpression() const { return expression; }
        inline std::shared_ptr<Carrot::Expression> getPrefix() const { return prefix; }

        std::any visit(BaseExpressionVisitor& visitor) override;
    };
}
