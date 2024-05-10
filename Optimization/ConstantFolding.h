//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include <functional>
#include "../AST/ASTVisitor/ASTVisitor.h"

class ConstantFolding : public ASTVisitor {
private:
	inline bool is_zero(NodeInt* node) {
		return node && node->value == 0;
	}
	inline bool is_zero(NodeReal* node) {
		return node && node->value == 0;
	}

public:
	void inline visit(NodeFunctionCall& node) override {
		// return immediately if the function is not a builtin function to save time
		if(!node.function->is_builtin) return;

		node.function->args->accept(*this);

		if(node.function->args->params.size() == 1) {
			if (node.function->args->params[0]->get_node_type() == NodeType::Int) {
				int32_t result = 0;
				auto int_node = cast_node<NodeInt>(node.function->args->params[0].get());
				// Definition der Funktions-Map für Integer-Operationen
				static std::unordered_map<std::string, std::function<int32_t(int32_t)>> int_functions = {
					{"inc", [](int32_t value) { return value + 1; }},
					{"dec", [](int32_t value) { return value - 1; }},
					{"abs", [](int32_t value) { return std::abs(value); }},
				};
				if (int_functions.find(node.function->name) != int_functions.end()) {
					result = int_functions[node.function->name](int_node->value);
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					node.replace_with(std::move(new_node));
					return;
				} else if(node.function->name == "real") {
					auto new_node = std::make_unique<NodeReal>(static_cast<double>(int_node->value), node.tok);
					node.replace_with(std::move(new_node));
					return;
				}
			} else if (node.function->args->params[0]->get_node_type() == NodeType::Real) {
				double result = 0;
				auto real_node = cast_node<NodeReal>(node.function->args->params[0].get());
				static std::unordered_map<std::string, std::function<double(double)>> real_functions = {
					// real number functions
					{"exp", [](double value) { return std::exp(value); }},
					{"sin", [](double value) { return std::sin(value); }},
					{"cos", [](double value) { return std::cos(value); }},
					// rounding commands
					{"floor", [](double value) { return std::floor(value); }},
					{"ceil", [](double value) { return std::ceil(value); }},
					{"round", [](double value) { return std::round(value); }}
				};
				if (real_functions.find(node.function->name) != real_functions.end()) {
					result = real_functions[node.function->name](real_node->value);
					auto new_node = std::make_unique<NodeReal>(result, node.tok);
					node.replace_with(std::move(new_node));
					return;
				} else if (node.function->name == "int") {
					auto new_node = std::make_unique<NodeInt>(static_cast<int32_t>(real_node->value), node.tok);
					node.replace_with(std::move(new_node));
					return;
				}
			}
		}

	};

    void inline visit(NodeUnaryExpr& node) override {
        node.operand->accept(*this);

		switch (node.operand->get_node_type()) {
			case NodeType::Int: {
				if (auto int_node = cast_node<NodeInt>(node.operand.get())) {
					if (node.op == token::SUB) {
						auto new_node = std::make_unique<NodeInt>(-int_node->value, node.tok);
						new_node->parent = node.parent;
						node.replace_with(std::move(new_node));
					}
				}
				break;
			}
			case NodeType::Real: {
				if (auto real_node = cast_node<NodeReal>(node.operand.get())) {
					if (node.op == token::SUB) {
						auto new_node = std::make_unique<NodeReal>(-real_node->value, node.tok);
						new_node->parent = node.parent;
						node.replace_with(std::move(new_node));
					}
				}
				break;
			}
			default: return;
		}
    }

	void inline visit(NodeBinaryExpr& node) override {
		node.left->accept(*this);
		node.right->accept(*this);

		NodeInt* right_int = nullptr;
		NodeInt* left_int = nullptr;
		NodeReal* right_real = nullptr;
		NodeReal* left_real = nullptr;

		if(node.right->get_node_type() == NodeType::Int) right_int = cast_node<NodeInt>(node.right.get());
		if(node.right->get_node_type() == NodeType::Int) left_int = cast_node<NodeInt>(node.left.get());
		if(node.right->get_node_type() == NodeType::Real) right_real = cast_node<NodeReal>(node.right.get());
		if(node.right->get_node_type() == NodeType::Real) left_real = cast_node<NodeReal>(node.left.get());

		auto error = CompileError(ErrorType::MathError,"","", node.tok);

		if(right_int || left_int) {
			if (contains(MATH_TOKENS, node.op) or contains(BITWISE_TOKENS, node.op)) {
				// constant folding
				if (left_int and right_int) {
					// Beide Operanden sind Integers. Führe die Operation aus und ersetze den Knoten.
					int32_t result = 0;
					auto static int_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
						{token::ADD, [](int32_t a, int32_t b) { return a + b; }},
						{token::SUB, [](int32_t a, int32_t b) { return a - b; }},
						{token::MULT, [](int32_t a, int32_t b) { return a * b; }},
						{token::DIV, [](int32_t a, int32_t b) { return a / b; }},
						{token::MODULO, [](int32_t a, int32_t b) { return a % b; }},
						{token::BIT_AND, [](int32_t a, int32_t b) { return a & b; }},
						{token::BIT_OR, [](int32_t a, int32_t b) { return a | b; }},
						{token::BIT_XOR, [](int32_t a, int32_t b) { return a ^ b; }}
					};
					if (int_operations.find(node.op) != int_operations.end()) {
						result = int_operations[node.op](left_int->value, right_int->value);
						auto new_node = std::make_unique<NodeInt>(result, node.tok);
						node.replace_with(std::move(new_node));
						return;
					}
				// division by zero
				} else if (is_zero(right_int) and node.op == token::DIV) {
					error.m_message = "Found division by zero. Result will be infinite.";
					error.exit();
				// multiplication with neutral element
				} else if (node.op == token::MULT && (is_zero(left_int) || is_zero(right_int))) {
					auto new_node = std::make_unique<NodeInt>(0, node.tok);
					node.replace_with(std::move(new_node));
					return;
				// 0 + var
				} else if (node.op == token::ADD and is_zero(left_int)) {
					node.replace_with(std::move(node.right));
					return;
				// var + 0
				} else if (node.op == token::ADD and is_zero(right_int)) {
					node.replace_with(std::move(node.left));
					return;
				}
			}
		} else if (right_real or left_real) {
			if (contains(MATH_TOKENS, node.op)) {
				// check division by zero
				if (node.op == token::DIV && is_zero(right_real)) {
					error.m_message = "Found division by zero. Result will be infinite.";
					error.exit();
				}
				// constant folding
				if (left_real and right_real) {
					double result = 0;
					auto static real_operations = std::unordered_map<token, std::function<double(double, double)>>{
						{token::ADD, [](double a, double b) { return a + b; }},
						{token::SUB, [](double a, double b) { return a - b; }},
						{token::MULT, [](double a, double b) { return a * b; }},
						{token::DIV, [](double a, double b) { return a / b; }},
						{token::MODULO, [](double a, double b) { return std::fmod(a, b); }}
					};
					if (real_operations.find(node.op) != real_operations.end()) {
						result = real_operations[node.op](left_real->value, right_real->value);
						auto new_node = std::make_unique<NodeReal>(result, node.tok);
						node.replace_with(std::move(new_node));
						return;
					}
				}
			}
		}
	}
};

