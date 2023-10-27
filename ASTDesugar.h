//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include <unordered_map>

inline std::vector<std::string> MATH_OPERATORS = {"-", "+", "/", "*", "mod"};

class ASTDesugar : public ASTVisitor {
public:
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;

//	void visit(NodeAssignStatement& node) override;
	/// alter for loops to while loops
	void visit(NodeForStatement& node) override;
};

