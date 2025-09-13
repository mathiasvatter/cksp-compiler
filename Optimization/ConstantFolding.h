//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include <functional>
#include "../AST/ASTVisitor/ASTOptimizations.h"

class ConstantFolding final : public ASTOptimizations {
	static bool is_zero(const NodeInt* node) {
		return node && node->value == 0;
	}
	static bool is_zero(const NodeReal* node) {
		return node && node->value == 0;
	}
	static std::unique_ptr<NodeInt> get_int(int32_t value, const Token& tok) {
		return std::make_unique<NodeInt>(value, tok);
	}
	static bool all_params_are_type(const NodeFunctionCall& node, NodeType type) {
		for(const auto & param : node.function->args->params) {
			if(param->get_node_type() != type) return false;
		}
		return true;
	}

public:

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	// check if variable reference is constant and can be substituted
	NodeAST* visit(NodeVariableRef& node) override {
		return node.try_constant_value_replace();
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		if (const auto definition = node.get_definition()) {
			definition->accept(*this);
		}
		// return immediately if the function is not a builtin function to save time
		if(node.kind != NodeFunctionCall::Kind::Builtin) return &node;

		node.function->args->accept(*this);

		if(node.function->get_num_args() == 1) {
			if(all_params_are_type(node, NodeType::ArrayRef)) {
//				if(node.function->name == "num_elements") {
//					auto array_ref = static_cast<NodeArrayRef*>(node.function->get_param(0).get());
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
				const auto int_node = node.function->get_arg(0)->cast<NodeInt>();
				// Definition der Funktions-Map für Integer-Operationen
				static std::unordered_map<std::string, std::function<int32_t(int32_t)>> int_functions = {
					{"inc", [](const int32_t value) { return value + 1; }},
					{"dec", [](const int32_t value) { return value - 1; }},
					{"abs", [](const int32_t value) { return std::abs(value); }},
				};
				if (int_functions.contains(node.function->name)) {
					int32_t result = int_functions[node.function->name](int_node->value);
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					new_node->ty = TypeRegistry::Integer;
					return node.replace_with(std::move(new_node));
				} else if(node.function->name == "real" or node.function->name == "int_to_real") {
					auto new_node = std::make_unique<NodeReal>(static_cast<double>(int_node->value), node.tok);
					new_node->ty = TypeRegistry::Real;
					return node.replace_with(std::move(new_node));
				// int(4) -> 4
				} else if (node.function->name == "int") {
					return node.replace_with(std::move(node.function->get_arg(0)));
				}
			} else if (all_params_are_type(node, NodeType::Real)) {
				const auto real_node = node.function->get_arg(0)->cast<NodeReal>();
				static std::unordered_map<std::string, std::function<double(double)>> real_functions = {
					// real number functions
					{"exp", [](const double value) { return std::exp(value); }},
					{"sin", [](const double value) { return std::sin(value); }},
					{"cos", [](const double value) { return std::cos(value); }},
					// rounding commands
					{"floor", [](const double value) { return std::floor(value); }},
					{"ceil", [](const double value) { return std::ceil(value); }},
					{"round", [](const double value) { return std::round(value); }}
				};
				if (real_functions.contains(node.function->name)) {
					double result = real_functions[node.function->name](real_node->value);
					auto new_node = std::make_unique<NodeReal>(result, node.tok);
					return node.replace_with(std::move(new_node));
				} else if (node.function->name == "int" or node.function->name == "real_to_int") {
					auto new_node = std::make_unique<NodeInt>(static_cast<int32_t>(real_node->value), node.tok);
					return node.replace_with(std::move(new_node));
				// real(4.0) -> 4.0
				} else if (node.function->name == "real") {
					return node.replace_with(std::move(node.function->get_arg(0)));
				}
			// check types -> real(~var) -> ~var; int($var) -> $var
			} else if (node.function->get_arg(0)->ty == TypeRegistry::Integer) {
				if (node.function->name == "int") {
					return node.replace_with(std::move(node.function->get_arg(0)));
				}
			} else if (node.function->get_arg(0)->ty == TypeRegistry::Real) {
				if (node.function->name == "real") {
					return node.replace_with(std::move(node.function->get_arg(0)));
				}
			}

		} else if(node.function->get_num_args() == 3) {
			if(node.function->name == "in_range") {
				if(all_params_are_type(node, NodeType::Int)) {
					const auto value = node.function->get_arg(0)->cast<NodeInt>();
					const auto min = node.function->get_arg(1)->cast<NodeInt>();
					const auto max = node.function->get_arg(2)->cast<NodeInt>();
					auto new_node = std::make_unique<NodeInt>(value->value >= min->value && value->value <= max->value, node.tok);
					return node.replace_with(std::move(new_node));
				} else if(all_params_are_type(node, NodeType::Real)) {
					const auto value = node.function->get_arg(0)->cast<NodeReal>();
					const auto min = node.function->get_arg(1)->cast<NodeReal>();
					const auto max = node.function->get_arg(2)->cast<NodeReal>();
					auto new_node = std::make_unique<NodeReal>(value->value >= min->value && value->value <= max->value, node.tok);
					return node.replace_with(std::move(new_node));
				}
			}
		} else if(node.function->get_num_args() == 2) {
			if(node.function->name == "sh_left" or node.function->name == "sh_right") {
				if(all_params_are_type(node, NodeType::Int)) {
					const auto value = node.function->get_arg(0)->cast<NodeInt>();
					const auto shift = node.function->get_arg(1)->cast<NodeInt>();
					int32_t result = 0;
					if(node.function->name == "sh_left") {
						result = value->value << shift->value;
					} else {
						result = value->value >> shift->value;
					}
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					return node.replace_with(std::move(new_node));
				}
			}
		}
		return &node;
	};

    NodeAST* visit(NodeUnaryExpr& node) override {
        node.operand->accept(*this);
		if (const auto int_node = node.operand->cast<NodeInt>()) {
			if (node.op == token::SUB) {
				auto new_node = std::make_unique<NodeInt>(-int_node->value, node.tok);
				new_node->ty = TypeRegistry::Integer;
				return node.replace_with(std::move(new_node));
			} else if (node.op == token::BOOL_NOT) {
				auto new_node = std::make_unique<NodeInt>(int_node->value == 0 ? 1 : 0, node.tok);
				new_node->ty = TypeRegistry::Integer;
				return node.replace_with(std::move(new_node));
			}
		} else if (const auto real_node = node.operand->cast<NodeReal>()) {
			if (node.op == token::SUB) {
				auto new_node = std::make_unique<NodeReal>(-real_node->value, node.tok);
				new_node->ty = TypeRegistry::Real;
				return node.replace_with(std::move(new_node));
			}
		}
		return &node;
    }

	NodeAST* visit(NodeBinaryExpr& node) override {
		node.left->accept(*this);
		node.right->accept(*this);

		auto right_int = node.right->cast<NodeInt>();
		auto left_int = node.left->cast<NodeInt>();
		auto right_real = node.right->cast<NodeReal>();
		auto left_real = node.left->cast<NodeReal>();

		if(right_int || left_int) {
			if (MATH_TOKENS.contains(node.op) or BITWISE_TOKENS.contains(node.op)) {
				// constant folding
				if (left_int and right_int) {
					// Beide Operanden sind Integers. Führe die Operation aus und ersetze den Knoten.
					auto static int_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
						{token::ADD, [](const int32_t a, const int32_t b) { return a + b; }},
						{token::SUB, [](const int32_t a, const int32_t b) { return a - b; }},
						{token::MULT, [](const int32_t a, const int32_t b) { return a * b; }},
						{token::DIV, [](const int32_t a, const int32_t b) { return a / b; }},
						{token::MODULO, [](const int32_t a, const int32_t b) { return a % b; }},
						{token::BIT_AND, [](const int32_t a, const int32_t b) { return a & b; }},
						{token::BIT_OR, [](const int32_t a, const int32_t b) { return a | b; }},
						{token::BIT_XOR, [](const int32_t a, const int32_t b) { return a ^ b; }}
					};
					if (int_operations.contains(node.op)) {
						int32_t result = int_operations[node.op](left_int->value, right_int->value);
						auto new_node = std::make_unique<NodeInt>(result, node.tok);
						new_node->ty = TypeRegistry::Integer;
						return node.replace_with(std::move(new_node));
					}
				// division by zero
				} else if (is_zero(right_int) and node.op == token::DIV) {
					auto error = CompileError(ErrorType::MathError,"","", node.tok);
					error.m_message = "Found division by zero. Result will be infinite.";
					error.exit();
				} else if (left_int or right_int) {
					// der bekannte  Integer
					NodeInt* known_int = left_int ? left_int : right_int;
					switch (node.op) {
						case token::ADD:
							if (known_int->value == 0) {
								// Absorbierendes Element, das Ergebnis ist immer der andere Ausdruck
								node.replace_with(left_int ? std::move(node.right) : std::move(node.left));
							}
							break;
						case token::SUB:
							// value - 0 = value
							if (right_int and right_int->value == 0) {
								node.replace_with(std::move(node.left));
							// 0 - value = - value
							} else if (left_int and left_int->value == 0) {
								node.replace_with(std::make_unique<NodeUnaryExpr>(token::SUB, std::move(node.right), node.tok));
							}
							break;
						case token::MULT:
							if (known_int->value == 0) {
								// Absorbierendes Element, das Ergebnis ist immer 0
								node.replace_with(get_int(0, node.tok));
							} else if (known_int->value == 1) {
								// Neutrales Element, das Ergebnis ist immer der andere Ausdruck
								node.replace_with(left_int ? std::move(node.right) : std::move(node.left));
							}
							break;
						case token::DIV:
							if (known_int->value == 0) {
								// Absorbierendes Element, das Ergebnis ist immer 0
								node.replace_with(get_int(0, node.tok));
							} else if (known_int->value == 1) {
								// Neutrales Element, das Ergebnis ist immer der andere Ausdruck
								node.replace_with(left_int ? std::move(node.right) : std::move(node.left));
							}
							break;
						case token::MODULO:
							if (known_int->value == 0) {
								// Absorbierendes Element, das Ergebnis ist immer 0
								node.replace_with(get_int(0, node.tok));
							}
							break;
						default: break;
					}
				}
			} else if (BOOL_TOKENS.contains(node.op)) {
				if (left_int and right_int) {
					auto static bool_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
						{token::BOOL_AND, [](const int32_t a, const int32_t b) { return (a && b) ? 1 : 0; }},
						{token::BOOL_OR, [](const int32_t a, const int32_t b) { return (a || b) ? 1 : 0; }},
						{token::BOOL_XOR, [](const int32_t a, const int32_t b) { return (a ^ b) ? 1 : 0; }},
					};
					if (bool_operations.contains(node.op)) {
						int32_t result = bool_operations[node.op](left_int->value, right_int->value);
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
					result->ty = TypeRegistry::Integer;
					return node.replace_with(std::move(result));
				}
			} else if (COMPARISON_TOKENS.contains(node.op)) {
				if (left_int and right_int) {
					auto static bool_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
						{token::LESS_THAN, [](const int32_t a, const int32_t b) { return (a < b) ? 1 : 0; }},
						{token::GREATER_THAN, [](const int32_t a, const int32_t b) { return (a > b) ? 1 : 0; }},
						{token::EQUAL, [](const int32_t a, const int32_t b) { return (a == b) ? 1 : 0; }},
						{token::NOT_EQUAL, [](const int32_t a, const int32_t b) { return (a != b) ? 1 : 0; }},
						{token::LESS_EQUAL, [](const int32_t a, const int32_t b) { return (a <= b) ? 1 : 0; }},
						{token::GREATER_EQUAL, [](const int32_t a, const int32_t b) { return (a >= b) ? 1 : 0; }},
					};
					if (bool_operations.contains(node.op)) {
						int32_t result = bool_operations[node.op](left_int->value, right_int->value);
						auto new_node = std::make_unique<NodeInt>(result, node.tok);
						new_node->ty = TypeRegistry::Integer;
						return node.replace_with(std::move(new_node));
					}
				}
			}
		} else if (right_real or left_real) {
			if (MATH_TOKENS.contains(node.op)) {
				// check division by zero
				if (node.op == token::DIV && is_zero(right_real)) {
					auto error = CompileError(ErrorType::MathError,"","", node.tok);
					error.m_message = "Found division by zero. Result will be infinite.";
					error.exit();
				}
				// constant folding
				if (left_real and right_real) {
					auto static real_operations = std::unordered_map<token, std::function<double(double, double)>>{
						{token::ADD, [](const double a, const double b) { return a + b; }},
						{token::SUB, [](const double a, const double b) { return a - b; }},
						{token::MULT, [](const double a, const double b) { return a * b; }},
						{token::DIV, [](const double a, const double b) { return a / b; }},
						{token::MODULO, [](const double a, const double b) { return std::fmod(a, b); }}
					};
					if (real_operations.contains(node.op)) {
						double result = real_operations[node.op](left_real->value, right_real->value);
						auto new_node = std::make_unique<NodeReal>(result, node.tok);
						new_node->ty = TypeRegistry::Real;
						return node.replace_with(std::move(new_node));
					}
				}
			} else if (COMPARISON_TOKENS.contains(node.op)) {
				if (left_real and right_real) {
					auto static bool_operations = std::unordered_map<token, std::function<int32_t(double, double)>>{
						{token::LESS_THAN, [](const double a, const double b) { return (a < b) ? 1 : 0; }},
						{token::GREATER_THAN, [](const double a, const double b) { return (a > b) ? 1 : 0; }},
						{token::EQUAL, [](const double a, const double b) { return (a == b) ? 1 : 0; }},
						{token::NOT_EQUAL, [](const double a, const double b) { return (a != b) ? 1 : 0; }},
						{token::LESS_EQUAL, [](const double a, const double b) { return (a <= b) ? 1 : 0; }},
						{token::GREATER_EQUAL, [](const double a, const double b) { return (a >= b) ? 1 : 0; }},
					};
					if (bool_operations.contains(node.op)) {
						int32_t result = bool_operations[node.op](left_real->value, left_real->value);
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

