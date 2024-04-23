//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "../AST/ASTVisitor.h"

class ConstantFolding : public ASTVisitor {
public:
	void inline visit(NodeBinaryExpr& node) override {
		node.left->accept(*this);
		node.right->accept(*this);

		auto right_int = cast_node<NodeInt>(node.right.get());
		auto left_int = cast_node<NodeInt>(node.left.get());
		auto right_real = cast_node<NodeReal>(node.right.get());
		auto left_real = cast_node<NodeReal>(node.left.get());

		if(get_token_type(MATH_OPERATORS, node.op) or get_token_type(BITWISE_OPERATORS, node.op)) {

			// constant folding
			if (left_int and right_int) {
				// Beide Operanden sind Integers. Führe die Operation aus und ersetze den Knoten.
				int32_t result = 0;
				auto int_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
					{token::ADD, [](int32_t a, int32_t b) { return a + b; }},
					{token::SUB, [](int32_t a, int32_t b) { return a - b; }},
					{token::MULT, [](int32_t a, int32_t b) { return a * b; }},
					{token::DIV, [](int32_t a, int32_t b) { return a / b; }},
					{token::MODULO, [](int32_t a, int32_t b) { return a % b; }},
					{token::BIT_AND, [](int32_t a, int32_t b) { return a & b; }},
					{token::BIT_OR, [](int32_t a, int32_t b) { return a | b; }},
					{token::BIT_XOR, [](int32_t a, int32_t b) { return a ^ b; }}
				};
				token tok = *get_token_type(ALL_OPERATORS, node.op);
				if (int_operations.find(tok) != int_operations.end()) {
					result = int_operations[tok](left_int->value, right_int->value);
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					new_node->parent = node.parent;
					node.replace_with(std::move(new_node));
					return;
				}
			}
			// division by zero
			if (right_int and get_token_type(MATH_OPERATORS, node.op) == token::DIV and right_int->value == 0) {
				CompileError(ErrorType::MathError,"Warning: Found division by zero.",node.tok.line,"","",node.tok.file).print();
				return;
			}
			if(left_int and left_int->value == 0 and get_token_type(MATH_OPERATORS, node.op) == token::MULT or right_int and right_int->value == 0 and
				get_token_type(MATH_OPERATORS, node.op) == token::MULT) {
				auto new_node = std::make_unique<NodeInt>(0, node.tok);
				new_node-> parent = node.parent;
				node.replace_with(std::move(new_node));
				return;
			}
			// 0 + var
			if(left_int and left_int->value == 0 and get_token_type(MATH_OPERATORS, node.op) == token::ADD) {
				node.replace_with(std::move(node.right));
				return;
			}
			// var + 0
			if(right_int and right_int->value == 0 and get_token_type(MATH_OPERATORS, node.op) == token::ADD) {
				node.replace_with(std::move(node.left));
				return;
			}
		}
		if(get_token_type(MATH_OPERATORS, node.op)) {
			// check division by zero
			if(right_real and node.op == "/" and right_real->value == (double)0) {
				CompileError(ErrorType::MathError, "Found division by zero. Result will be infinite.", node.tok.line, "", "", node.tok.file).print();
				exit(EXIT_FAILURE);
			}
			// constant folding
			if (left_real and right_real) {
				double result = 0;
				auto real_operations = std::unordered_map<token, std::function<double(double, double)>>{
					{token::ADD, [](double a, double b) { return a + b; }},
					{token::SUB, [](double a, double b) { return a - b; }},
					{token::MULT, [](double a, double b) { return a * b; }},
					{token::DIV, [](double a, double b) { return a / b; }},
					{token::MODULO, [](double a, double b) { return std::fmod(a, b); }}
				};
				token tok = *get_token_type(MATH_OPERATORS, node.op);
				if (real_operations.find(tok) != real_operations.end()) {
					result = real_operations[tok](left_real->value, right_real->value);
					auto new_node = std::make_unique<NodeReal>(result, node.tok);
					new_node->parent = node.parent;
					node.replace_with(std::move(new_node));
					return;
				}
			}
		}
	}
};

