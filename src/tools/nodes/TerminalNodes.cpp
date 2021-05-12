//
// Created by jglrxavpok on 11/05/2021.
//

#include "TerminalNodes.h"

std::shared_ptr<Carrot::Expression> Tools::TerminalNode::toExpression() const {
    return getExpressionsFromInput()[0];
}

