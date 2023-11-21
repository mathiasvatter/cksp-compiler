//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTDesugar.h"


ASTDesugar::ASTDesugar(const std::vector<std::unique_ptr<NodeFunctionHeader>> &mBuiltinFunctions) : m_builtin_functions(
        mBuiltinFunctions) {
}

void ASTDesugar::visit(NodeProgram& node) {
    m_function_definitions = std::move(node.function_definitions);
    m_in_init_callback = false;
    for(auto & callback : node.callbacks) {
        if(callback->begin_callback == "on init") m_in_init_callback = true;
        callback->accept(*this);
    }
    if(!m_in_init_callback) {
        CompileError(ErrorType::SyntaxError, "Unable to compile. Missing <init callback>.", -1, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    node.function_definitions = std::move(m_function_definitions);
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
    } else {
        if(contains(VAR_IDENT, node.name[0])) {
            auto identifier = node.name[0];
            node.name = node.name.erase(0,1);
            token token_type = get_token_type(TYPES, std::to_string(identifier));
            node.type = token_to_type(token_type);
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
    } else {
        if(contains(VAR_IDENT, node.name[0])) {
            auto identifier = node.name[0];
            node.name = node.name.erase(0,1);
            token token_type = get_token_type(TYPES, std::to_string(identifier));
            node.type = token_to_type(token_type);
        }
    }
    // add prefixes
    if(!m_prefixes.empty()) {
        node.name = m_prefixes.top() + "." + node.name;
    }
}

void ASTDesugar::visit(NodeFunctionCall& node) {
    if(node.is_call and !node.function->args->params.empty()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect amount of function arguments when using <call>.", node.tok.line, "0", std::to_string(node.function->args->params.size()), node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    if(std::find(m_function_call_stack.begin(), m_function_call_stack.end(), node.function->name) != m_function_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::SyntaxError,"Recursive function call detected. Calling functions inside their definition is not allowed.", node.tok.line, "", node.function->name, node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    node.function->accept(*this);

    // substitution start
    if (auto node_function_def = get_function_definition(node.function.get())) {
        m_function_call_stack.push_back(node.function->name);
        node_function_def->parent = node.parent;
        auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
        node_statement_list->parent = node.parent;
        if (!node.function->args->params.empty()) {
            auto substitution_vec = get_substitution_vector(node_function_def->header.get(), node.function.get());
            m_substitution_stack.push(std::move(substitution_vec));
        }
        for(auto& stmt: node_function_def->body) {
            stmt->accept(*this);
            stmt->parent = node_statement_list.get();
        }
        if (!node.function->args->params.empty()) m_substitution_stack.pop();
        node_statement_list->statements = std::move(node_function_def->body);
        node.replace_with(std::move(node_statement_list));
        m_function_call_stack.pop_back();
    } else if (!is_builtin_function(node.function.get())) {
        CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).print();
        exit(EXIT_FAILURE);
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

    // while statement
	auto node_while_statement = std::make_unique<NodeWhileStatement>(node.tok);

    if(!node.step) {
        // function call
        std::string function_name = "inc";
        if (node.to.type == DOWNTO) function_name = "dec";
        std::vector<std::unique_ptr<NodeAST>> args;
        args.push_back(std::move(function_var));
        auto inc_statement = make_function_call(function_name, std::move(args), node_while_statement.get(), node.tok);
        inc_statement->update_parents(node_while_statement.get());
        node.statements.push_back(std::move(inc_statement));
    } else {
        auto assign_var2 = function_var->clone();
        auto inc_expression = make_binary_expr(Integer, "+", std::move(function_var), std::move(node.step), &node, node.tok);
        auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(std::move(assign_var2), std::move(inc_expression), node.tok);
        auto node_statement = statement_wrapper(std::move(node_inc_statement), node_while_statement.get());
        node_statement->update_parents(node_while_statement.get());
        node.statements.push_back(std::move(node_statement));
    }

    // handle parenting of while statement body
	for(auto & stmt : node.statements) {
		stmt->parent = node_while_statement.get();
		stmt->accept(*this);
	}

    // handle while condition
    std::string comparison_op = "<=";
    if(node.to.type == DOWNTO) comparison_op = ">=";
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

bool ASTDesugar::is_builtin_function(NodeFunctionHeader *function) {
    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
                               return (func->name == function->name and
                               func->arg_ast_types.size() == function->args->params.size());
                           });
    return it != m_builtin_functions.end();
}

void ASTDesugar::handle_function_overrides() {
    std::vector<NodeFunctionDefinition*> override_functions;
    for (const auto& function : m_function_definitions) {
        if (function->override) {
            override_functions.push_back(function.get());
        }
    }



}



