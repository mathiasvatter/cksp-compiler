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

void ASTDesugar::visit(NodeAssignStatement &node) {
    if(node.array_variable->params.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect assign statement syntax. There are more values to assign than assignees.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    bool is_declaration = false;
    auto* declareStmt = dynamic_cast<NodeDeclareStatement*>(node.parent);
    if (declareStmt != nullptr) is_declaration = true;
    bool is_const_block = false;
    auto* constStmt = dynamic_cast<NodeConstStatement*>(node.parent->parent);
    if(constStmt != nullptr) is_const_block = true;
    if(is_const_block)
        for(auto & arr_var : node.array_variable->params) {
            auto* var = dynamic_cast<NodeVariable*>(arr_var.get());
            if(var == nullptr) {
                CompileError(ErrorType::SyntaxError,"Found incorrect const block syntax. No arrays allowed in const blocks.", node.tok.line, "", "", node.tok.file).print();
                exit(EXIT_FAILURE);
            }
            if(var->var_type != Mutable) {
                CompileError(ErrorType::SyntaxError,"Warning: Found incorrect const block syntax. No other variable types allowed in const blocks.", node.tok.line, "", "", node.tok.file).print();
            }
            var->var_type = Const;
            var->name = constStmt->prefix + "." + var->name;
        }

    std::vector<std::unique_ptr<NodeSingleAssignStatement>> assign_statements;
    for(auto &arr_var : node.array_variable->params) {
        auto node_single_assign_stmt = std::make_unique<NodeSingleAssignStatement>(node.tok);
        arr_var->parent = node_single_assign_stmt.get();
        node_single_assign_stmt->array_variable = std::move(arr_var);
        assign_statements.push_back(std::move(node_single_assign_stmt));
    }
    std::vector<std::shared_ptr<NodeAST>> values;
    for(auto &ass : node.assignee->params) {
        values.push_back(std::shared_ptr<NodeAST>(std::move(ass)));
    }
    // in case there is nothing assigned
    if(!node.assignee->params.empty())
        // there were more variables given than values -> repeat the last value
        while(values.size() < assign_statements.size()) {
            values.push_back(values.back());
        }

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);

    std::vector<std::unique_ptr<NodeAST>> single_statements;
    for(int i = 0; i<assign_statements.size(); i++) {
        auto &stmt = assign_statements[i];
        // in case there is nothing assigned -> nullptr
        if (!node.assignee->params.empty()) {
            auto &val = values[i];
            stmt->assignee = val;
            stmt->assignee->parent = stmt.get();
        }
        if(is_declaration) {
            auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(node.tok);
            node_declare_statement->to_be_declared = std::move(stmt->array_variable);
            node_declare_statement->to_be_declared->parent = node_declare_statement.get();
            node_declare_statement->assignee = std::move(stmt->assignee);
            if(node_declare_statement->assignee != nullptr)
                node_declare_statement->assignee->parent = node_declare_statement.get();
            single_statements.push_back(std::move(node_declare_statement));
        } else single_statements.push_back(std::move(stmt));
    }

    for(auto & stmt : single_statements) {
        auto node_statement = std::make_unique<NodeStatement>(std::move(stmt), node.tok);
        node_statement->statement->parent = node_statement.get();
        node_statement->parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(node_statement));
    }
    node_statement_list->parent = node.parent;
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugar::visit(NodeDeclareStatement& node) {
    node.statement->accept(*this);
    node.statement->parent = node.parent;
    node.replace_with(std::move(node.statement));
}

void ASTDesugar::visit(NodeConstStatement& node) {
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    std::vector<int32_t> const_indexes;
    for(int i = 0; i<node.constants.size(); i++){
        node.constants[i]->accept(*this);
        const_indexes.push_back(i);
        node_statement_list->statements.push_back(std::move(node.constants[i]));
    }
    auto array = make_declare_array(node.prefix, node.constants.size(), const_indexes, node_statement_list.get());
    node_statement_list->statements.push_back(std::move(array));
    auto constant = make_declare_variable(node.prefix+".SIZE", node.constants.size(), Const, node_statement_list.get());
    node_statement_list->statements.push_back(std::move(constant));
    node_statement_list->parent = node.parent;
    node.replace_with(std::move(node_statement_list));

}

std::unique_ptr<NodeInt> ASTDesugar::make_int(int32_t value, NodeAST* parent) {
    auto node_int = std::make_unique<NodeInt>(value, parent->tok);
    node_int->parent = parent;
    return node_int;
}

std::unique_ptr<NodeParamList> ASTDesugar::make_init_array_list(const std::vector<int32_t>& values, NodeAST* parent) {
    auto node_array_init_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, parent->tok));
    node_array_init_list -> parent = parent;
    for(auto & i : values) {
        node_array_init_list->params.push_back(make_int(i, node_array_init_list.get()));
    }
    return node_array_init_list;
}

std::unique_ptr<NodeStatement> ASTDesugar::make_declare_array(const std::string& name, int32_t size, const std::vector<int32_t>& values, NodeAST* parent) {
    auto sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, parent->tok));
    sizes->params.push_back(make_int(size, parent));
    auto indexes  = std::unique_ptr<NodeParamList>(new NodeParamList({}, parent->tok));
    auto node_array = std::make_unique<NodeArray>(false, name, Array, std::move(sizes), std::move(indexes), parent->tok);
    node_array->sizes->parent = node_array.get();
    node_array->indexes->parent = node_array.get();
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), make_init_array_list(values, parent), parent->tok);
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    node_declare_statement->assignee->parent = node_declare_statement.get();
    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declare_statement), parent->tok);
    node_statement->statement->parent = node_statement.get();
    node_statement->parent = parent;
    return node_statement;
}

std::unique_ptr<NodeStatement> ASTDesugar::make_declare_variable(const std::string& name, int32_t value, VarType type, NodeAST* parent) {
    auto node_variable = std::make_unique<NodeVariable>(false, name, type, parent->tok);
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_variable), make_int(value, parent), parent->tok);
    node_declare_statement->assignee->parent = node_declare_statement.get();
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declare_statement), parent->tok);
    node_statement->parent = parent;
    return node_statement;
}



void ASTDesugar::visit(NodeForStatement& node) {
	// check if there is only one var and assignee
	if(node.iterator->array_variable->params.size() != 1 or node.iterator->assignee->params.size() != 1) {
		CompileError(ErrorType::SyntaxError, "Found incorrect for-loop syntax.", node.tok.line, "one iterator per for-loop", "multiple iterators", node.tok.file).print();
		exit(EXIT_FAILURE);
	}
	node.iterator_end->accept(*this);

    // function arg
	std::unique_ptr<NodeAST> iterator_var;
    std::unique_ptr<NodeAST> assign_var;
	if (auto var = dynamic_cast<NodeVariable *>(node.iterator->array_variable->params[0].get())) {
        iterator_var = std::make_unique<NodeVariable>(var->is_persistent, var->name, var->var_type, var->tok);
        assign_var = std::make_unique<NodeVariable>(var->is_persistent, var->name, var->var_type, var->tok);
	} else if(auto arr = dynamic_cast<NodeArray *>(node.iterator->array_variable->params[0].get())) {
        iterator_var = std::make_unique<NodeArray>(arr->is_persistent, arr->name, arr->var_type, std::move(arr->sizes), std::move(arr->indexes), arr->tok);
        assign_var = std::make_unique<NodeArray>(arr->is_persistent, arr->name, arr->var_type, std::move(arr->sizes), std::move(arr->indexes), arr->tok);
	}
	std::string comparison_op = "<=";
    std::string function_name = "inc";
	if(node.to.type == DOWNTO) {
		comparison_op = ">=";
        function_name = "dec";
	}
    // while statement
	auto node_while_statement = std::make_unique<NodeWhileStatement>(node.tok);

    // function call
    std::vector<std::unique_ptr<NodeAST>> args;
    args.push_back(std::move(node.iterator->array_variable->params[0]));
    auto inc_statement = make_function_call(function_name, std::move(args), node_while_statement.get(), node.tok);

    // handle parenting of while statement body
	for(auto & stmt : node.statements) {
		stmt->accept(*this);
		stmt->parent = node_while_statement.get();
	}

    // make comparison expression
    auto comparison = make_binary_expr(Comparison, comparison_op, std::move(iterator_var), std::move(node.iterator_end), node_while_statement.get(), node.tok);

	node_while_statement->condition = std::move(comparison);
	node_while_statement->statements = std::move(node.statements);
	node_while_statement->statements.push_back(std::move(inc_statement));

    auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(node.tok);
    node_assign_statement->array_variable = std::move(assign_var);
    node_assign_statement->assignee = std::move(node.iterator->assignee->params[0]);
    auto node_statement = std::make_unique<NodeStatement>(std::move(node_assign_statement), node.tok);
    node_statement->statement->parent = node_statement.get();

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    node_statement->parent = node_statement_list.get();
    node_statement_list->statements.push_back(std::move(node_statement));
    node_while_statement->parent = node_statement_list.get();
    node_statement_list->statements.push_back(std::move(node_while_statement));

	node.replace_with(std::move(node_statement_list));
}

std::unique_ptr<NodeStatement> ASTDesugar::make_function_call(const std::string& name, std::vector<std::unique_ptr<NodeAST>> args, NodeAST* parent, Token tok) {
    auto func_args = std::unique_ptr<NodeParamList>(new NodeParamList(std::move(args), tok));
    for(auto & arg : func_args->params) {
        arg->parent = func_args.get();
    }
    // make function header
    auto func = std::make_unique<NodeFunctionHeader>(name, std::move(func_args), tok);
    func->args->parent = func.get();

    // make function call out of header
    auto func_call = std::make_unique<NodeFunctionCall>(false, std::move(func), tok);
    func_call->function->parent = func_call.get();

    // make statement out of function call
    auto function_call = std::make_unique<NodeStatement>(std::move(func_call), tok);
    function_call -> parent = parent;
    return function_call;
}

std::unique_ptr<NodeBinaryExpr>ASTDesugar::make_binary_expr(ASTType type, const std::string& op, std::unique_ptr<NodeAST> lhs, std::unique_ptr<NodeAST> rhs, NodeAST* parent, Token tok) {
    // make comparison expression
    auto comparison = std::make_unique<NodeBinaryExpr>(op, std::move(lhs), std::move(rhs), tok);
    comparison->type = type;
    comparison->parent = parent;
    comparison->left->parent = comparison.get();
    comparison->right->parent = comparison.get();
    return comparison;
}





