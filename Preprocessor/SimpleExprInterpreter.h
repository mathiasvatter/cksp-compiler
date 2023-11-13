//
// Created by Mathias Vatter on 13.11.23.
//

#pragma once

#include "PreAST.h"

class SimpleExprInterpreter {
public:
    explicit SimpleExprInterpreter() = default;
    Result<int> evaluate_int_expression(std::unique_ptr<PreNodeAST>& root);
};


