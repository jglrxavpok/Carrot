//
// Created by jglrxavpok on 11/05/2021.
//

#include "TerminalNodes.h"

std::shared_ptr<Carrot::Expression> Tools::TerminalNode::toExpression(uint32_t pinIndex) const {
    std::vector<std::shared_ptr<Carrot::Expression>> expressions;

    std::uint32_t index = 0;
    for(const auto& input : getExpressionsFromInput()) {
        if(input != nullptr) {
            expressions.push_back(std::make_shared<Carrot::SetVariableExpression>(getInternalName(nodeType), input, index));
        }

        index++;
    }
    return make_shared<Carrot::CompoundExpression>(std::move(expressions));
}

