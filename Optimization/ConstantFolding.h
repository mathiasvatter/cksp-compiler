//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include <functional>
#include "../AST/ASTVisitor/ASTOptimizations.h"

class ConstantFolding : public ASTOptimizations {
private:
	static inline bool is_zero(NodeInt* node) {
		return node && node->value == 0;
	}
	static inline bool is_zero(NodeReal* node) {
		return node && node->value == 0;
	}
	inline static std::unique_ptr<NodeInt> get_int(int32_t value, const Token& tok) {
		return std::make_unique<NodeInt>(value, tok);
	}
	inline static bool all_params_are_type(const NodeFunctionCall& node, NodeType type) {
		for(auto & param : node.function->header->args->params) {
			if(param->get_node_type() != type) return false;
		}
		return true;
	}

public:
	inline NodeAST* visit(NodeFunctionCall& node) override {
		// return immediately if the function is not a builtin function to save time
		if(node.kind != NodeFunctionCall::Kind::Builtin) return &node;

		node.function->header->args->accept(*this);

		if(node.function->get_num_args() == 1) {
			if(all_params_are_type(node, NodeType::ArrayRef)) {
//				if(node.function->name == "num_elements") {
//					auto array_ref = static_cast<NodeArrayRef*>(node.function->get_arg(0).get());
//					// array only has SIZE constant if user defined
//					if(array_ref->kind != NodeReference::Kind::User) return &node;
//
//					auto node_size_const = std::make_unique<NodeVariableRef>(
//						array_ref->name + ".SIZE", array_ref->tok
//						);
//					node_size_const->ty = TypeRegistry::Integer;
//					node_size_const->data_type = DataType::Const;
//					return node.replace_with(std::move(node_size_const));
//				}
			} else if (all_params_are_type(node, NodeType::Int)) {
				int32_t result = 0;
				auto int_node = static_cast<NodeInt*>(node.function->get_arg(0).get());
				// Definition der Funktions-Map für Integer-Operationen
				static std::unordered_map<std::string, std::function<int32_t(int32_t)>> int_functions = {
					{"inc", [](int32_t value) { return value + 1; }},
					{"dec", [](int32_t value) { return value - 1; }},
					{"abs", [](int32_t value) { return std::abs(value); }},
				};
				if (int_functions.find(node.function->name) != int_functions.end()) {
					result = int_functions[node.function->name](int_node->value);
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					return node.replace_with(std::move(new_node));
				} else if(node.function->name == "real") {
					auto new_node = std::make_unique<NodeReal>(static_cast<double>(int_node->value), node.tok);
					return node.replace_with(std::move(new_node));
				}
			} else if (all_params_are_type(node, NodeType::Real)) {
				double result = 0;
				auto real_node = static_cast<NodeReal*>(node.function->get_arg(0).get());
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
					return node.replace_with(std::move(new_node));
				} else if (node.function->name == "int") {
					auto new_node = std::make_unique<NodeInt>(static_cast<int32_t>(real_node->value), node.tok);
					return node.replace_with(std::move(new_node));
				}
			}
		} else if(node.function->get_num_args() == 3) {
			if(node.function->name == "in_range") {
				if(all_params_are_type(node, NodeType::Int)) {
					auto value = static_cast<NodeInt*>(node.function->get_arg(0).get());
					auto min = static_cast<NodeInt*>(node.function->get_arg(1).get());
					auto max = static_cast<NodeInt*>(node.function->get_arg(2).get());
					auto new_node = std::make_unique<NodeInt>(value->value >= min->value && value->value <= max->value, node.tok);
					return node.replace_with(std::move(new_node));
				} else if(all_params_are_type(node, NodeType::Real)) {
					auto value = static_cast<NodeReal*>(node.function->get_arg(0).get());
					auto min = static_cast<NodeReal*>(node.function->get_arg(1).get());
					auto max = static_cast<NodeReal*>(node.function->get_arg(2).get());
					auto new_node = std::make_unique<NodeReal>(value->value >= min->value && value->value <= max->value, node.tok);
					return node.replace_with(std::move(new_node));
				}
			}
		}
		return &node;
	};

    NodeAST* visit(NodeUnaryExpr& node) override {
        node.operand->accept(*this);

		switch (node.operand->get_node_type()) {
			case NodeType::Int: {
				if (auto int_node = static_cast<NodeInt*>(node.operand.get())) {
					if (node.op == token::SUB) {
						auto new_node = std::make_unique<NodeInt>(-int_node->value, node.tok);
						new_node->parent = node.parent;
						return node.replace_with(std::move(new_node));
					} else if (node.op == token::BOOL_NOT) {
						auto new_node = std::make_unique<NodeInt>(int_node->value == 0 ? 1 : 0, node.tok);
						new_node->parent = node.parent;
						return node.replace_with(std::move(new_node));
					}
				}
				break;
			}
			case NodeType::Real: {
				if (auto real_node = static_cast<NodeReal*>(node.operand.get())) {
					if (node.op == token::SUB) {
						auto new_node = std::make_unique<NodeReal>(-real_node->value, node.tok);
						new_node->parent = node.parent;
						return node.replace_with(std::move(new_node));
					}
				}
				break;
			}
			default: return &node;
		}
		return &node;
    }

	inline NodeAST* visit(NodeBinaryExpr& node) override {
		node.left->accept(*this);
		node.right->accept(*this);

		NodeInt* right_int = nullptr;
		NodeInt* left_int = nullptr;
		NodeReal* right_real = nullptr;
		NodeReal* left_real = nullptr;

		if(node.right->get_node_type() == NodeType::Int) right_int = static_cast<NodeInt*>(node.right.get());
		if(node.left->get_node_type() == NodeType::Int) left_int = static_cast<NodeInt*>(node.left.get());
		if(node.right->get_node_type() == NodeType::Real) right_real = static_cast<NodeReal*>(node.right.get());
		if(node.left->get_node_type() == NodeType::Real) left_real = static_cast<NodeReal*>(node.left.get());


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
						return node.replace_with(std::move(new_node));
					}
				// division by zero
				} else if (is_zero(right_int) and node.op == token::DIV) {
					auto error = CompileError(ErrorType::MathError,"","", node.tok);
					error.m_message = "Found division by zero. Result will be infinite.";
					error.exit();
				}
			} else if (contains(BOOL_TOKENS, node.op)) {
				if (left_int and right_int) {
					int32_t result = 0;
					auto static bool_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
						{token::BOOL_AND, [](int32_t a, int32_t b) { return (a && b) ? 1 : 0; }},
						{token::BOOL_OR, [](int32_t a, int32_t b) { return (a || b) ? 1 : 0; }},
						{token::BOOL_XOR, [](int32_t a, int32_t b) { return (a ^ b) ? 1 : 0; }},
					};
					if (bool_operations.find(node.op) != bool_operations.end()) {
						result = bool_operations[node.op](left_int->value, right_int->value);
						auto new_node = std::make_unique<NodeInt>(result, node.tok);
						new_node->ty = TypeRegistry::Integer;
						return node.replace_with(std::move(new_node));
					}
				// e.g. if(($mw_active = 0) and 1)
				} else if (left_int or right_int) {
					// der bekannte Integer
					NodeInt* known_int = left_int ? left_int : right_int;
					// der unbekannte Ausdruck
					std::unique_ptr<NodeAST> other_expr = left_int ? std::move(node.right) : std::move(node.left);
					std::unique_ptr<NodeAST> result = std::move(other_expr);
					switch (node.op) {
						case token::BOOL_AND:
							if (known_int->value == 0) {
								// Absorbierendes Element, das Ergebnis ist immer 0
								result = get_int(0, node.tok);
							}
							break;
						case token::BOOL_OR:
							if (known_int->value == 1) {
								// Absorbierendes Element, das Ergebnis ist immer 1
								result = get_int(1, node.tok);
							}
							break;
						case token::BOOL_XOR:
							if (known_int->value == 1) {
								// Umkehrendes Element, kehre den anderen Ausdruck um
								result = std::make_unique<NodeUnaryExpr>(token::BOOL_NOT, std::move(other_expr), node.tok);
							}
							break;
						default: break;
					}
					return node.replace_with(std::move(result));
				}
			} else if (contains(COMPARISON_TOKENS, node.op)) {
				if (left_int and right_int) {
					int32_t result = 0;
					auto static bool_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
						{token::LESS_THAN, [](int32_t a, int32_t b) { return (a < b) ? 1 : 0; }},
						{token::GREATER_THAN, [](int32_t a, int32_t b) { return (a > b) ? 1 : 0; }},
						{token::EQUAL, [](int32_t a, int32_t b) { return (a == b) ? 1 : 0; }},
						{token::NOT_EQUAL, [](int32_t a, int32_t b) { return (a != b) ? 1 : 0; }},
						{token::LESS_EQUAL, [](int32_t a, int32_t b) { return (a <= b) ? 1 : 0; }},
						{token::GREATER_EQUAL, [](int32_t a, int32_t b) { return (a >= b) ? 1 : 0; }},
					};
					if (bool_operations.find(node.op) != bool_operations.end()) {
						result = bool_operations[node.op](left_int->value, right_int->value);
						auto new_node = std::make_unique<NodeInt>(result, node.tok);
						new_node->ty = TypeRegistry::Integer;
						return node.replace_with(std::move(new_node));
					}
				}
			}
		} else if (right_real or left_real) {
			if (contains(MATH_TOKENS, node.op)) {
				// check division by zero
				if (node.op == token::DIV && is_zero(right_real)) {
					auto error = CompileError(ErrorType::MathError,"","", node.tok);
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
						return node.replace_with(std::move(new_node));
					}
				}
			} else if (contains(COMPARISON_TOKENS, node.op)) {
				if (left_real and right_real) {
					int32_t result = 0;
					auto static bool_operations = std::unordered_map<token, std::function<int32_t(double, double)>>{
						{token::LESS_THAN, [](double a, double b) { return (a < b) ? 1 : 0; }},
						{token::GREATER_THAN, [](double a, double b) { return (a > b) ? 1 : 0; }},
						{token::EQUAL, [](double a, double b) { return (a == b) ? 1 : 0; }},
						{token::NOT_EQUAL, [](double a, double b) { return (a != b) ? 1 : 0; }},
						{token::LESS_EQUAL, [](double a, double b) { return (a <= b) ? 1 : 0; }},
						{token::GREATER_EQUAL, [](double a, double b) { return (a >= b) ? 1 : 0; }},
					};
					if (bool_operations.find(node.op) != bool_operations.end()) {
						result = bool_operations[node.op](left_real->value, left_real->value);
						auto new_node = std::make_unique<NodeInt>(result, node.tok);
						new_node->ty = TypeRegistry::Integer;
						return node.replace_with(std::move(new_node));
					}
				}
			}
		}
		return &node;
	}
};

