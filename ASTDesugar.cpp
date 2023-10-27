//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTDesugar.h"

void ASTDesugar::visit(NodeBinaryExpr& node) {
	// Besuche zuerst die Kinder des Knotens
	if (node.left) node.left->accept(*this);
	if (node.right) node.right->accept(*this);

	if(contains(MATH_OPERATORS, node.op) or node.op == ".and." or node.op == ".or" or node.op == ".xor") {
		if (auto right_int = dynamic_cast<NodeInt *>(node.right.get())) {
			if (node.op == "/" and right_int->value == 0) {
				CompileError(ErrorType::MathError,"Warning: Found division by zero.",node.tok.line,"","",node.tok.file).print();
				return;
			}
		}
		// constant folding
		if (auto left_int = dynamic_cast<NodeInt *>(node.left.get())) {
			if (auto right_int = dynamic_cast<NodeInt *>(node.right.get())) {
				// Beide Operanden sind Integers. Führe die Operation aus und ersetze den Knoten.
				int32_t result = 0;
				auto int_operations = std::unordered_map<std::string, std::function<int32_t(int32_t, int32_t)>>{
					{"+", [](int32_t a, int32_t b) { return a + b; }},
					{"-", [](int32_t a, int32_t b) { return a - b; }},
					{"*", [](int32_t a, int32_t b) { return a * b; }},
					{"/", [](int32_t a, int32_t b) { return a / b; }},
					{"mod", [](int32_t a, int32_t b) { return a % b; }},
					{".and.", [](int32_t a, int32_t b) { return a & b; }},
					{".or.", [](int32_t a, int32_t b) { return a | b; }},
					{".xor.", [](int32_t a, int32_t b) { return a ^ b; }}
				};
				if (int_operations.find(node.op) != int_operations.end()) {
					result = int_operations[node.op](left_int->value, right_int->value);
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					node.replace_with(std::move(new_node));
				}
			}
		}
	}
	if(contains(MATH_OPERATORS, node.op)) {
		if (auto right_real = dynamic_cast<NodeReal *>(node.right.get())) {
			if(node.op == "/" and right_real->value == (double)0) {
				CompileError(ErrorType::MathError, "Found division by zero. Result will be infinite.", node.tok.line, "", "", node.tok.file).print();
				exit(EXIT_FAILURE);
			}
		}
		// constant folding
		if (auto left_real = dynamic_cast<NodeReal *>(node.left.get())) {
			if (auto right_real = dynamic_cast<NodeReal *>(node.right.get())) {
				double result = 0;
				auto real_operations = std::unordered_map<std::string, std::function<double(double, double)>>{
					{"+", [](double a, double b) { return a + b; }},
					{"-", [](double a, double b) { return a - b; }},
					{"*", [](double a, double b) { return a * b; }},
					{"/", [](double a, double b) { return a / b; }},
					{"mod", [](double a, double b) { return std::fmod(a, b); }}
				};
				if (real_operations.find(node.op) != real_operations.end()) {
					result = real_operations[node.op](left_real->value, right_real->value);
					auto new_node = std::make_unique<NodeInt>(result, node.tok);
					node.replace_with(std::move(new_node));
				}
			}
		}
	}
}

//void ASTDesugar::visit(NodeAssignStatement &node) {
//	ASTVisitor::visit(node);
//}



void ASTDesugar::visit(NodeForStatement& node) {
	// check if there is only one var and assignee
	if(node.iterator->array_variable->params.size() != 1 or node.iterator->assignee->params.size() != 1) {
		CompileError(ErrorType::SyntaxError, "Found incorrect for-loop syntax.", node.tok.line, "one iterator per for-loop", "multiple iterators", node.tok.file).print();
		exit(EXIT_FAILURE);
	}

	node.iterator_end->accept(*this);

	std::unique_ptr<NodeAST> iterator_var;
	if (auto var = dynamic_cast<NodeVariable *>(node.iterator->array_variable->params[0].get())) {
		iterator_var = std::make_unique<NodeVariable>(var->is_persistent, var->name, var->var_type, var->tok);
	} else if(auto arr = dynamic_cast<NodeArray *>(node.iterator->array_variable->params[0].get())) {
		iterator_var = std::make_unique<NodeArray>(arr->is_persistent, arr->name, arr->var_type, std::move(arr->sizes), std::move(arr->indexes), arr->tok);
	}
	std::string comparison_op = "<=";
	auto func_args = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
	func_args->params.push_back(std::move(node.iterator->array_variable->params[0]));
	func_args->params[0]->parent = func_args.get();
	auto inc = std::make_unique<NodeFunctionHeader>("inc", std::move(func_args), node.tok);
	inc->args->parent = inc.get();
	if(node.to.type == DOWNTO) {
		comparison_op = ">=";
		inc->name = "dec";
	}
	auto inc_call = std::make_unique<NodeFunctionCall>(false, std::move(inc), node.tok);
	inc_call->function->parent = inc_call.get();
	auto inc_statement = std::make_unique<NodeStatement>(std::move(inc_call), node.tok);

	auto node_while_statement = std::make_unique<NodeWhileStatement>(node.tok);
	node_while_statement->parent = node.parent;
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
		stmt->parent = node_while_statement.get();
	}
	auto comparison = std::make_unique<NodeBinaryExpr>(comparison_op, std::move(iterator_var), std::move(node.iterator_end), node.tok);
	comparison->type = ASTType::Comparison;
	comparison->parent = node_while_statement.get();
	comparison->left->parent = comparison.get();
	comparison->right->parent = comparison.get();

	node_while_statement->condition = std::move(comparison);
	node_while_statement->statements = std::move(node.statements);
	node_while_statement->statements.push_back(std::move(inc_statement));
	node.replace_with(std::move(node_while_statement));
}





