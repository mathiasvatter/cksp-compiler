//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTDesugar.h"


ASTDesugar::ASTDesugar() {

}

void ASTDesugar::visit(NodeBinaryExpr& node) {
	// Besuche zuerst die Kinder des Knotens
	if (node.left) node.left->accept(*this);
	if (node.right) node.right->accept(*this);

    auto right_int = cast_node<NodeInt>(node.right.get());
    auto left_int = cast_node<NodeInt>(node.left.get());
    auto right_real = cast_node<NodeReal>(node.right.get());
    auto left_real = cast_node<NodeReal>(node.left.get());

	if(contains(MATH_OPERATORS, node.op) or node.op == ".and." or node.op == ".or" or node.op == ".xor") {
        // division by zero
        if (right_int and node.op == "/" and right_int->value == 0) {
            CompileError(ErrorType::MathError,"Warning: Found division by zero.",node.tok.line,"","",node.tok.file).print();
            return;
        }
        if(left_int and left_int->value == 0 and node.op == "*" or right_int and right_int->value == 0 and node.op == "*") {
            auto new_node = std::make_unique<NodeInt>(0, node.tok);
            new_node-> parent = node.parent;
            node.replace_with(std::move(new_node));
            return;
        }
		// constant folding
		if (left_int and right_int) {
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
                new_node->parent = node.parent;
                node.replace_with(std::move(new_node));
            }
		}
	}
	if(contains(MATH_OPERATORS, node.op)) {
        // check division by zero
        if(right_real and node.op == "/" and right_real->value == (double)0) {
            CompileError(ErrorType::MathError, "Found division by zero. Result will be infinite.", node.tok.line, "", "", node.tok.file).print();
            exit(EXIT_FAILURE);
        }
		// constant folding
		if (left_real and right_real) {
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
                auto new_node = std::make_unique<NodeReal>(result, node.tok);
                new_node->parent = node.parent;
                node.replace_with(std::move(new_node));
            }
		}
	}
}

void ASTDesugar::visit(NodeArray& node) {
    node.sizes->accept(*this);
    node.indexes->accept(*this);
    // substitution
    if(!m_substitution_stack.empty()) {
        if(auto substitute = get_substitute(node.name)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
    // add prefixes
    if(!m_prefixes.empty()) {
        node.name = m_prefixes.top() + "." + node.name;
    }
}

void ASTDesugar::visit(NodeVariable& node) {
    // substitution
    if(!m_substitution_stack.empty()) {
        if(auto substitute = get_substitute(node.name)) {
            node.replace_with(std::move(substitute));
            return;
        }
    }
    // add prefixes
    if(!m_prefixes.empty()) {
        node.name = m_prefixes.top() + "." + node.name;
    }
}

void ASTDesugar::visit(NodeProgram& node) {
//    for(auto & function_definition : node.function_definitions) {
//        function_definition->accept(*this);
//    }
    m_function_definitions = std::move(node.function_definitions);
    bool has_init_callback = false;
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
        if(callback->begin_callback == "on init") has_init_callback = true;
    }
    if(!has_init_callback) {
        CompileError(ErrorType::SyntaxError, "Unable to compile. Missing <init callback>.", -1, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    node.function_definitions = std::move(m_function_definitions);
}

void ASTDesugar::visit(NodeFunctionCall& node) {
    if(node.is_call and !node.function->args->params.empty()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect amount of function arguments when using <call>.", node.tok.line, "0", std::to_string(node.function->args->params.size()), node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    node.function->accept(*this);

    if (!node.function->args->params.empty())
        // substitution start
        if (auto node_function_def = get_function_definition(node.function.get())) {
            node_function_def->parent = node.parent;
            auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
            node_statement_list->parent = node.parent;
            auto substitution_vec = get_substitution_vector(node_function_def->header.get(), node.function.get());
            m_substitution_stack.push(std::move(substitution_vec));
//            node_function_def->accept(*this);
            for(auto& stmt: node_function_def->body) {
                stmt->accept(*this);
                stmt->parent = node_statement_list.get();
            }
            m_substitution_stack.pop();
            node_statement_list->statements = std::move(node_function_def->body);
            node.replace_with(std::move(node_statement_list));
        }
}

std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> ASTDesugar::get_substitution_vector(NodeFunctionHeader* definition, NodeFunctionHeader* call) {
    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> substitution_vector;
    for(int i= 0; i<definition->args->params.size(); i++) {
        auto &var = definition->args->params[i];
        std::pair<std::string, std::unique_ptr<NodeAST>> pair;
        if(auto node_variable = cast_node<NodeVariable>(var.get())) {
            pair.first = node_variable->name;
        } else if (auto node_array = cast_node<NodeArray>(var.get())) {
            pair.first = node_array->name;
        } else {
            CompileError(ErrorType::SyntaxError,
             "Unable to substitute function arguments. Only <keywords> can be substituted.", definition->tok.line, "<keyword>", var->tok.val,definition->tok.file).print();
            exit(EXIT_FAILURE);
        }
        pair.second = std::move(call->args->params[i]);
        substitution_vector.push_back(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<NodeAST> ASTDesugar::get_substitute(const std::string& name) {
    for(auto & pair : m_substitution_stack.top()) {
        if(pair.first == name) {
            return pair.second->clone();
        }
    }
    return nullptr;
}


void ASTDesugar::visit(NodeSingleDeclareStatement& node) {
    node.to_be_declared ->accept(*this);
    if(node.assignee)
        node.assignee -> accept(*this);
}

void ASTDesugar::visit(NodeSingleAssignStatement& node) {
    node.array_variable ->accept(*this);
    node.assignee -> accept(*this);
}


void ASTDesugar::visit(NodeAssignStatement &node) {
    if(node.array_variable->params.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect assign statement syntax. There are more values to assign than assignees.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
//    node.array_variable->accept(*this);
//	node.assignee->accept(*this);
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
    // there were more variables given than values -> repeat the last value
    while(values.size() < assign_statements.size()) {
        values.push_back(values.back()->clone());
    }

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);

    for(int i = 0; i<assign_statements.size(); i++) {
        auto &stmt = assign_statements[i];
        auto &val = values[i];
        stmt->assignee = val;
        stmt->assignee->parent = stmt.get();
        node_statement_list->statements.push_back(std::move(
                statement_wrapper(std::move(stmt), node_statement_list.get())));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugar::visit(NodeDeclareStatement& node) {
    if(node.to_be_declared->params.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect declare statement syntax. There are more values to assign than to be declared.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
//    node.to_be_declared->accept(*this);
//    node.assignee->accept(*this);
    std::vector<std::unique_ptr<NodeSingleDeclareStatement>> declare_statements;
    for(auto &declaration : node.to_be_declared->params) {
        auto node_single_declare_stmt = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        declaration->parent = node_single_declare_stmt.get();
        node_single_declare_stmt->to_be_declared = std::move(declaration);
        declare_statements.push_back(std::move(node_single_declare_stmt));
    }
    std::vector<std::shared_ptr<NodeAST>> values;
    for(auto &ass : node.assignee->params) {
        values.push_back(std::shared_ptr<NodeAST>(std::move(ass)));
    }
    // in case there is nothing assigned
    if(!node.assignee->params.empty())
        // there were more variables given than values -> repeat the last value
        while(values.size() < declare_statements.size()) {
            values.push_back(values.back()->clone());
        }

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    // get to_be_declared and their values together and put them in to NodeStatement
    for(int i = 0; i<declare_statements.size(); i++) {
        auto &stmt = declare_statements[i];
        // in case there is nothing assigned -> nullptr
        if (!node.assignee->params.empty()) {
            auto &val = values[i];
            stmt->assignee = val;
            stmt->assignee->parent = stmt.get();
        }
        node_statement_list->statements.push_back(std::move(
                statement_wrapper(std::move(stmt), node_statement_list.get())));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));

}

void ASTDesugar::visit(NodeFamilyStatement& node) {
    std::string pref = node.prefix;
    if(!m_prefixes.empty()) pref = m_prefixes.top() + "." + node.prefix;
    m_prefixes.push(pref);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    for(auto &member : node.members) {
        member->accept(*this);
        member->parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(member));
    }
    node_statement_list->parent = node.parent;
    node.replace_with(std::move(node_statement_list));
    m_prefixes.pop();
}

void ASTDesugar::visit(NodeConstStatement& node) {
    std::string pref = node.prefix;
    if(!m_prefixes.empty()) pref = m_prefixes.top() + "." + node.prefix;
    m_prefixes.push(pref);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    std::vector<int32_t> const_indexes;
    int32_t iter = 0;
    int32_t pre = 0;
    for(int i = 0; i<node.constants.size(); i++){
        node.constants[i]->accept(*this);
        // check constants and give them values
        auto stmt_list = cast_node<NodeStatementList>(node.constants[i]->statement.get());
        for(auto &stmt : stmt_list->statements) {
            auto node_stmt = cast_node<NodeStatement>(stmt.get());
            auto declare_stmt = cast_node<NodeSingleDeclareStatement>(node_stmt->statement.get());
            if(!declare_stmt->assignee)
                declare_stmt->assignee = make_int(pre+iter, declare_stmt);
            else {
                auto node_int = cast_node<NodeInt>(declare_stmt->assignee.get());
                if(!node_int) {
                    CompileError(ErrorType::SyntaxError,
                 "Found incorrect <const block> syntax. Only integers can be assigned in <const blocks>.", node.tok.line,"", "", node.tok.file).print();
                    exit(EXIT_FAILURE);
                }
                pre = node_int->value;
                iter = 0;
            }
            auto var = cast_node<NodeVariable>(declare_stmt->to_be_declared.get());
            if (!var) {
                CompileError(ErrorType::SyntaxError,
                 "Found incorrect <const block> syntax. Only variables allowed in <const blocks>.", node.tok.line,"", "", node.tok.file).print();
                exit(EXIT_FAILURE);
            }
            if (var->var_type != Mutable) {
                CompileError(ErrorType::SyntaxError,
             "Warning: Found incorrect const block syntax. No other variable types allowed in const blocks. Casted variable type to <const>.",node.tok.line, "", "", node.tok.file).print();
            }
            var->type = Integer;
            var->var_type = Const;
//            var->name = node.prefix + "." + var->name;
        }

        const_indexes.push_back(pre+iter);
        node.constants[i] -> parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(node.constants[i]));
        iter++;
    }
    auto array = make_declare_array(node.prefix, node.constants.size(), const_indexes, node_statement_list.get());
    node_statement_list->statements.push_back(std::move(array));
    auto constant = make_declare_variable(node.prefix+".SIZE", node.constants.size(), Const, node_statement_list.get());
    node_statement_list->statements.push_back(std::move(constant));
    node_statement_list->parent = node.parent;
    node.replace_with(std::move(node_statement_list));
    m_prefixes.pop();
}

void ASTDesugar::visit(NodeForStatement& node) {
	// check if there is only one var and assignee
	if(node.iterator->array_variable->params.size() != 1 or node.iterator->assignee->params.size() != 1) {
		CompileError(ErrorType::SyntaxError, "Found incorrect for-loop syntax.", node.tok.line, "one iterator per for-loop", "multiple iterators", node.tok.file).print();
		exit(EXIT_FAILURE);
	}
	node.iterator_end->accept(*this);
    node.iterator->array_variable->accept(*this);
    node.iterator->assignee->accept(*this);
    // function arg
	std::unique_ptr<NodeAST> iterator_var = node.iterator->array_variable->params[0]->clone();
    std::unique_ptr<NodeAST> assign_var = iterator_var->clone();
    std::unique_ptr<NodeAST> function_var = iterator_var->clone();

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
    args.push_back(std::move(function_var));
    auto inc_statement = make_function_call(function_name, std::move(args), node_while_statement.get(), node.tok);
    node.statements.push_back(std::move(inc_statement));
    // handle parenting of while statement body
	for(auto & stmt : node.statements) {
		stmt->parent = node_while_statement.get();
		stmt->accept(*this);
	}

    // make comparison expression
    auto comparison = make_binary_expr(Comparison, comparison_op, std::move(iterator_var), std::move(node.iterator_end), node_while_statement.get(), node.tok);

	node_while_statement->condition = std::move(comparison);
	node_while_statement->statements = std::move(node.statements);

    auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(node.tok);
    node_assign_statement->array_variable = std::move(assign_var);
    node_assign_statement->assignee = std::move(node.iterator->assignee->params[0]);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_statement_list.get()));

    node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_statement), node_statement_list.get()));
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
    node_array->type = Integer;
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), make_init_array_list(values, parent), parent->tok);
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    node_declare_statement->assignee->parent = node_declare_statement.get();
    return statement_wrapper(std::move(node_declare_statement), parent);
}

std::unique_ptr<NodeStatement> ASTDesugar::make_declare_variable(const std::string& name, int32_t value, VarType type, NodeAST* parent) {
    auto node_variable = std::make_unique<NodeVariable>(false, name, type, parent->tok);
    node_variable->type = Integer;
    auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_variable), make_int(value, parent), parent->tok);
    node_declare_statement->assignee->parent = node_declare_statement.get();
    node_declare_statement->to_be_declared->parent = node_declare_statement.get();
    return statement_wrapper(std::move(node_declare_statement), parent);
}

std::unique_ptr<NodeFunctionDefinition> ASTDesugar::get_function_definition(NodeFunctionHeader *function_header) {
    for(auto & function_def : m_function_definitions) {
        if(function_def->header->name == function_header->name) {
            if(function_def->header->args->params.size() == function_header->args->params.size()) {
                function_def->is_used = true;
                auto copy = function_def->clone();
                copy->update_parents(nullptr);
                return std::unique_ptr<NodeFunctionDefinition>(static_cast<NodeFunctionDefinition*>(copy.release()));
            }
        }
    }
    return nullptr;
}

template<typename T>std::unique_ptr<NodeStatement> ASTDesugar::statement_wrapper(std::unique_ptr<T> node, NodeAST* parent) {
    auto node_statement = std::make_unique<NodeStatement>(std::move(node), node->tok);
    node_statement->statement->parent = node_statement.get();
    node_statement->parent = parent;
    return node_statement;
}





