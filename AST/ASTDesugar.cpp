//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTDesugar.h"


ASTDesugar::ASTDesugar(const std::vector<std::unique_ptr<NodeFunctionHeader>> &mBuiltinFunctions) : m_builtin_functions(
        mBuiltinFunctions) {
}

void ASTDesugar::visit(NodeProgram& node) {
    m_function_definitions = std::move(node.function_definitions);
//    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
//    node.function_definitions = std::move(function_definitions);

    // check for init callback; get pointer to init callback
    for(auto & callback : node.callbacks) {
        if(callback->begin_callback == "on init") m_init_callback = callback.get();
    }
    if(!m_init_callback) {
        CompileError(ErrorType::SyntaxError, "Unable to compile. Missing <init callback>.", -1, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    for(auto & function : m_function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            function->body->accept(*this);
        }
    }
    for(auto & function : m_function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            node.function_definitions.push_back(std::move(function));
        }
    }
    m_init_callback->statements->statements.insert(m_init_callback->statements->statements.begin(),
                                                   std::make_move_iterator(m_declare_statements_to_move.begin()),
                                                   std::make_move_iterator(m_declare_statements_to_move.end()));
}

void ASTDesugar::visit(NodeCallback& node) {
    m_current_callback = &node;
    m_in_init_callback = false;
    if(&node == m_init_callback) m_in_init_callback = true;
    if(node.callback_id)
        node.callback_id->accept(*this);
    node.statements->accept(*this);
}

void ASTDesugar::visit(NodeBinaryExpr& node) {
	node.left->accept(*this);
	node.right->accept(*this);

    auto right_int = cast_node<NodeInt>(node.right.get());
    auto left_int = cast_node<NodeInt>(node.left.get());
    auto right_real = cast_node<NodeReal>(node.right.get());
    auto left_real = cast_node<NodeReal>(node.left.get());

	if(contains(MATH_OPERATORS, node.op) or contains(BITWISE_OPERATORS, node.op)) {
        // division by zero
        if (right_int and get_token_type(MATH_OPERATORS, node.op) == DIV and right_int->value == 0) {
            CompileError(ErrorType::MathError,"Warning: Found division by zero.",node.tok.line,"","",node.tok.file).print();
            return;
        }
        if(left_int and left_int->value == 0 and get_token_type(MATH_OPERATORS, node.op) == MULT or right_int and right_int->value == 0 and
                get_token_type(MATH_OPERATORS, node.op) == MULT) {
            auto new_node = std::make_unique<NodeInt>(0, node.tok);
            new_node-> parent = node.parent;
            node.replace_with(std::move(new_node));
            return;
        }
		// constant folding
		if (left_int and right_int) {
            // Beide Operanden sind Integers. Führe die Operation aus und ersetze den Knoten.
            int32_t result = 0;
            auto int_operations = std::unordered_map<token, std::function<int32_t(int32_t, int32_t)>>{
                {ADD, [](int32_t a, int32_t b) { return a + b; }},
                {SUB, [](int32_t a, int32_t b) { return a - b; }},
                {MULT, [](int32_t a, int32_t b) { return a * b; }},
                {DIV, [](int32_t a, int32_t b) { return a / b; }},
                {MODULO, [](int32_t a, int32_t b) { return a % b; }},
                {BIT_AND, [](int32_t a, int32_t b) { return a & b; }},
                {BIT_OR, [](int32_t a, int32_t b) { return a | b; }},
                {BIT_XOR, [](int32_t a, int32_t b) { return a ^ b; }}
            };
            if (int_operations.find(get_token_type(ALL_OPERATORS, node.op)) != int_operations.end()) {
                result = int_operations[get_token_type(ALL_OPERATORS, node.op)](left_int->value, right_int->value);
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
            auto real_operations = std::unordered_map<token, std::function<double(double, double)>>{
                {ADD, [](double a, double b) { return a + b; }},
                {SUB, [](double a, double b) { return a - b; }},
                {MULT, [](double a, double b) { return a * b; }},
                {DIV, [](double a, double b) { return a / b; }},
                {MODULO, [](double a, double b) { return std::fmod(a, b); }}
            };
            if (real_operations.find(get_token_type(MATH_OPERATORS, node.op)) != real_operations.end()) {
                result = real_operations[get_token_type(MATH_OPERATORS, node.op)](left_real->value, right_real->value);
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
        if (auto substitute = get_substitute(node.name)) {
//            substitute->update_parents(node.parent);
//            substitute->accept(*this);
//            node.replace_with(std::move(substitute));
            node.name = substitute->get_string();
            return;
        }
    }
    if(contains(VAR_IDENT, node.name[0])) {
        auto identifier = node.name[0];
        node.name = node.name.erase(0,1);
        token token_type = get_token_type(TYPES, std::to_string(identifier));
        node.type = token_to_type(token_type);
    }
    // add prefixes
    if(!m_family_prefixes.empty() and is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
        node.name = m_family_prefixes.top() + "." + node.name;
    }
    if(!m_const_prefixes.empty()) {
        node.name = m_const_prefixes.top() + "." + node.name;
    }
}

void ASTDesugar::visit(NodeFunctionHeader& node) {
    node.args->accept(*this);
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.name)) {
            node.name = substitute->get_string();
            return;
        }
    }
}

void ASTDesugar::visit(NodeVariable& node) {
    // substitution
    if(!m_substitution_stack.empty()) {
        if(auto substitute = get_substitute(node.name)) {
            substitute->update_parents(node.parent);
//            substitute->accept(*this);
            node.replace_with(std::move(substitute));
            return;
        }
    }
    if(contains(VAR_IDENT, node.name[0])) {
        std::string identifier(1, node.name[0]);
        node.name = node.name.erase(0,1);
        token token_type = get_token_type(TYPES, identifier);
        node.type = token_to_type(token_type);
    }
    // add prefixes
    if(!m_family_prefixes.empty() and is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
        node.name = m_family_prefixes.top() + "." + node.name;
    }
    if(!m_const_prefixes.empty()) {
        node.name = m_const_prefixes.top() + "." + node.name;
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
        node_function_def->is_used = true;
        m_function_call_stack.push_back(node.function->name);
        node_function_def->parent = node.parent;
        if (!node.function->args->params.empty()) {
            auto substitution_vec = get_substitution_vector(node_function_def->header.get(), node.function.get());
            m_substitution_stack.push(std::move(substitution_vec));
        }
        m_processing_function = true;
        node_function_def->body->update_token_data(node.tok);
        node_function_def->body->accept(*this);
        if (!node.function->args->params.empty()) m_substitution_stack.pop();
        m_processing_function = false;
        m_function_call_stack.pop_back();
        // has return variable
        if(node_function_def->return_variable.has_value()) {
            if(is_instance_of<NodeStatementList>(node.parent->parent)) {
                CompileError(ErrorType::SyntaxError,"Function returns a value. Move function into assign statement.", node.tok.line, node_function_def->return_variable.value()->get_string(), "<assignment>", node.tok.file).print();
                exit(EXIT_FAILURE);
            }
            // inlining
            if(node_function_def->body->statements.size() == 1) {
                if(auto node_statement = cast_node<NodeStatement>(node_function_def->body->statements[0].get())) {
                    if(auto node_assignment = cast_node<NodeSingleAssignStatement>(node_statement->statement.get())) {
                        if(node_assignment->array_variable->get_string() == node_function_def->return_variable.value()->get_string()) {
                            node.replace_with(std::move(node_assignment->assignee));
                            return;
                        } else {
                            CompileError(ErrorType::SyntaxError,"Given return variable of function could not be found in function body.", node.tok.line, node_function_def->return_variable.value()->get_string(), node_assignment->array_variable->get_string(), node.tok.file).print();
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            } else if(node_function_def->body->statements.size() > 1) {
                if(is_instance_of<NodeSingleDeclareStatement>(node.parent) or is_instance_of<NodeSingleAssignStatement>(node.parent)) {
                    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> substitution_vec;
                    if (auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent)) {
                        substitution_vec.emplace_back(node_function_def->return_variable.value()->get_string(),
                                                      node_declaration->to_be_declared->clone());
                    } else if (auto node_assignment = cast_node<NodeSingleAssignStatement>(node.parent)) {
                        substitution_vec.emplace_back(node_function_def->return_variable.value()->get_string(),
                                                      node_assignment->array_variable->clone());
                    }
                    m_substitution_stack.push(std::move(substitution_vec));
                    node_function_def->body->accept(*this);
                    m_substitution_stack.pop();
                    node.parent->replace_with(std::move(node_function_def->body));
                    return;
                } else {
                    CompileError(ErrorType::SyntaxError,"Function can not be inlined since it consists of too many lines. Move function into assign statement.", node.tok.line, "<assignment>", "", node.tok.file).print();
                    exit(EXIT_FAILURE);
                }
            }
        }
        node.replace_with(std::move(node_function_def->body));
    } else if (auto builtin_func = get_builtin_function(node.function.get())) {
        node.function->type = builtin_func->type;
        node.function->arg_ast_types = builtin_func->arg_ast_types;
        node.function->arg_var_types = builtin_func->arg_var_types;
    } else {
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
    const auto & vector = m_substitution_stack.top();
    auto it = std::find_if(vector.begin(), vector.end(),
                           [&](const std::pair<std::string, std::unique_ptr<NodeAST>> &pair) {
                               return pair.first == name;
                           });
    if(it != vector.end()) {
        return vector[std::distance(vector.begin(), it)].second->clone();
    }
    return nullptr;
}


void ASTDesugar::visit(NodeSingleDeclareStatement& node) {
    node.to_be_declared ->accept(*this);
    if(node.assignee)
        node.assignee -> accept(*this);

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    // check if is_persistent
    bool is_persistent = false;
    auto node_array = cast_node<NodeArray>(node.to_be_declared.get());
    if(node_array) is_persistent = node_array->is_persistent;
    auto node_variable = cast_node<NodeVariable>(node.to_be_declared.get());
    if(node_variable) is_persistent = node_variable->is_persistent;

    NodeParamList* param_list = nullptr;
    if(node.assignee) param_list = cast_node<NodeParamList>(node.assignee.get());

    // make param list if it is array declaration
    if(node_array) {
        if(node.assignee and !param_list) {
            auto node_param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.assignee->tok));
            node_param_list->params.push_back(std::move(node.assignee));
            node_param_list->parent = &node;
            node.assignee = std::move(node_param_list);
        }
        // array size is not empty
        if(!node_array->sizes->params.empty()) {
            node_array->dimensions = node_array->sizes->params.size();
            // multidimensional array
            if (node_array->sizes->params.size() > 1) {
//                node_array->name = "_"+node_array->name;
                auto node_expression = create_right_nested_binary_expr(node_array->sizes->params, 0, "*", node_array->tok);
                node_expression->parent = node_array->sizes.get();
                for(int i = 0; i<node_array->sizes->params.size(); i++) {
                    auto node_var = std::make_unique<NodeVariable>(false, node_array->name+".SIZE_D"+std::to_string(i+1), Const, node.tok);
                    auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_var), node_array->sizes->params[i]->clone(), node.tok);
                    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declaration), node.tok);
                    node_statement_list->statements.push_back(std::move(node_statement));
                }
                node_array->indexes->params.clear();
                node_array->indexes->params.push_back(std::move(node_expression));
            }
        } else {
            if(!node.assignee) {
                CompileError(ErrorType::SyntaxError,"Unable to infer array size.", node.tok.line, "initializer list", "",node.tok.file).print();
                exit(EXIT_FAILURE);
            }
            // if size is empty -> get it from declaration
            if(param_list) {
                auto node_int = make_int((int32_t) param_list->params.size(), node_array->sizes.get());
                node_array->sizes->params.push_back(std::move(node_int));
            }
        }
    } else if(node_variable) {
        // a list of values is assigned to a declared variable
        if(param_list and param_list->params.size() > 1) {
            CompileError(ErrorType::SyntaxError,"Unable to assign a list of values to variable.", node.tok.line, "single value", "list of values",node.tok.file).print();
            exit(EXIT_FAILURE);
        }
    }

    node_statement_list->statements.push_back(statement_wrapper(node.clone(), node.parent));
    // add make_persistent and read_persistent_var
    if(is_persistent) {
        add_vector_to_statement_list(node_statement_list, add_read_functions(node.to_be_declared.get(), node_statement_list.get()));
    }
    node_statement_list->update_parents(node.parent);

    if(m_processing_function and !m_in_init_callback) {
        auto node_dead_end = std::make_unique<NodeDeadEnd>(node.tok);
        for (auto & stmt : node_statement_list->statements) {
            m_declare_statements_to_move.push_back(std::move(stmt));
        }
        node.replace_with(std::move(node_dead_end));
    } else {
        node.replace_with(std::move(node_statement_list));
    }

}

std::unique_ptr<NodeAST> ASTDesugar::create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok) {
    // Basisfall: Wenn nur ein Element übrig ist, gib dieses zurück.
    if (index >= nodes.size() - 1) {
        return nodes[index]->clone();
    }
    // Erstelle die rechte Seite der Expression rekursiv.
    auto right = create_right_nested_binary_expr(nodes, index + 1, op, tok);
    // Kombiniere das aktuelle Element mit der rechten Seite in einer NodeBinaryExpr.
    return std::make_unique<NodeBinaryExpr>(op, nodes[index]->clone(), std::move(right), tok);
}

void ASTDesugar::visit(NodeSingleAssignStatement& node) {
    node.array_variable ->accept(*this);
    node.assignee -> accept(*this);
}

void ASTDesugar::visit(NodeParamList& node) {
    for(auto & param : node.params) {
        param->accept(*this);
    }
    // in case it is a double param_list [[0,1,2,3]] move params up to parent
    if(auto node_param_list = cast_node<NodeParamList>(node.parent)) {
        if(node_param_list->params.size() == 1) {
            node_param_list->params.insert(node_param_list->params.begin(),std::make_move_iterator(node.params.begin()),std::make_move_iterator(node.params.end()));
            node_param_list->params.pop_back();
        }
    }
}

void ASTDesugar::visit(NodeAssignStatement &node) {
    if(node.array_variable->params.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect assign statement syntax. There are more values to assign than assignees.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    std::vector<std::unique_ptr<NodeSingleAssignStatement>> assign_statements;
    for(auto &arr_var : node.array_variable->params) {
        auto node_single_assign_stmt = std::make_unique<NodeSingleAssignStatement>(node.tok);
        arr_var->parent = node_single_assign_stmt.get();
        node_single_assign_stmt->array_variable = std::move(arr_var);
        assign_statements.push_back(std::move(node_single_assign_stmt));
    }
    std::vector<std::unique_ptr<NodeAST>> values;
    for(auto &ass : node.assignee->params) {
        values.push_back(std::move(ass));
    }
    // there were more variables given than values -> repeat the last value
    while(values.size() < assign_statements.size()) {
        values.push_back(values.back()->clone());
    }

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);

    for(int i = 0; i<assign_statements.size(); i++) {
        auto &stmt = assign_statements[i];
        auto &val = values[i];
        stmt->assignee = std::move(val);
        stmt->assignee->parent = stmt.get();
        node_statement_list->statements.push_back(std::move(
                statement_wrapper(std::move(stmt), node_statement_list.get())));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugar::visit(NodeDeclareStatement& node) {
    if(node.to_be_declared->params.size() < node.assignee->params.size()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect declare statement syntax. There are more values to assign than to be declared.", node.tok.line, "", "", node.tok.file).print();
        exit(EXIT_FAILURE);
    }

    std::vector<std::unique_ptr<NodeSingleDeclareStatement>> declare_statements;
    for(auto &declaration : node.to_be_declared->params) {
        auto node_single_declare_stmt = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        declaration->parent = node_single_declare_stmt.get();
        node_single_declare_stmt->to_be_declared = std::move(declaration);
        declare_statements.push_back(std::move(node_single_declare_stmt));
    }
    std::vector<std::unique_ptr<NodeAST>> values;
    for(auto &ass : node.assignee->params) {
        values.push_back(std::move(ass));
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
            stmt->assignee = std::move(val);
            stmt->assignee->parent = stmt.get();
        }
        node_statement_list->statements.push_back(std::move(
                statement_wrapper(std::move(stmt), node_statement_list.get())));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

std::vector<std::unique_ptr<NodeStatement>> ASTDesugar::add_read_functions(NodeAST* var, NodeAST* parent) {
    std::vector<std::unique_ptr<NodeStatement>> statements;

    std::string persistent1 = "make_persistent";
    std::string persistent2 = "read_persistent_var";

    auto node_function = get_builtin_function(persistent1)->clone();
    auto make_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    make_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(make_persistent), parent));

    node_function = get_builtin_function(persistent2)->clone();
    auto read_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    read_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(read_persistent), parent));
    return std::move(statements);
}


void ASTDesugar::visit(NodeFamilyStatement& node) {
    std::string pref = node.prefix;
    if(!m_family_prefixes.empty()) pref = m_family_prefixes.top() + "." + node.prefix;
    m_family_prefixes.push(pref);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    for(auto &member : node.members) {
//        member->accept(*this);
        member->parent = node_statement_list.get();
        node_statement_list->statements.push_back(std::move(member));
    }
    node_statement_list->parent = node.parent;
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
    m_family_prefixes.pop();
}

void ASTDesugar::visit(NodeConstStatement& node) {
    std::string pref = node.prefix;
    if(!m_const_prefixes.empty()) pref = m_const_prefixes.top() + "." + node.prefix;
    m_const_prefixes.push(pref);
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
//    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
    m_const_prefixes.pop();
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
        inc_statement->update_parents(&node);
        node.statements.push_back(std::move(inc_statement));
    } else {
        auto assign_var2 = function_var->clone();
        auto inc_expression = make_binary_expr(Integer, "+", std::move(function_var), std::move(node.step), &node, node.tok);
        auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(std::move(assign_var2), std::move(inc_expression), node.tok);
        auto node_statement = statement_wrapper(std::move(node_inc_statement), node_while_statement.get());
        node_statement->update_parents(&node);
        node.statements.push_back(std::move(node_statement));
    }

    // handle parenting of while statement body
	for(auto & stmt : node.statements) {
//		stmt->update_parents(node_while_statement.get());
//		stmt->accept(*this);
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
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
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

NodeFunctionHeader* ASTDesugar::get_builtin_function(NodeFunctionHeader *function) {
    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
                               return (func->name == function->name and
                               func->arg_ast_types.size() == function->args->params.size());
                           });
    if(it != m_builtin_functions.end()) {
        return m_builtin_functions[std::distance(m_builtin_functions.begin(), it)].get();
    }
    return nullptr;
}

NodeFunctionHeader* ASTDesugar::get_builtin_function(const std::string &function) {
    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
                               return (func->name == function);
                           });
    if(it != m_builtin_functions.end()) {
        return m_builtin_functions[std::distance(m_builtin_functions.begin(), it)].get();
    }
    return nullptr;
}


void ASTDesugar::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
//		stmt->parent = &node;
    }
    for(int i=0; i<node.statements.size(); ++i) {
        if(auto node_statement_list = cast_node<NodeStatementList>(node.statements[i]->statement.get())) {
            // Wir speichern die Statements der inneren NodeStatementList
            auto& inner_statements = node_statement_list->statements;
            // Fügen Sie die inneren Statements an der aktuellen Position ein
            node.statements.insert(
                    node.statements.begin() + i + 1,
                    std::make_move_iterator(inner_statements.begin()),
                    std::make_move_iterator(inner_statements.end())
            );
            // Entfernen Sie das ursprüngliche NodeStatementList-Element
            node.statements.erase(node.statements.begin() + i);
            // Anpassen des Indexes, um die eingefügten Elemente zu berücksichtigen
            i += inner_statements.size() - 1;
            // Die inneren Statements sind jetzt leer, da sie verschoben wurden
            inner_statements.clear();
        }
    }
    node.update_parents(&node);
}



void ASTDesugar::add_vector_to_statement_list(std::unique_ptr<NodeStatementList> &list, std::vector<std::unique_ptr<NodeStatement>> stmts) {
    list->statements.insert(list->statements.end(),std::make_move_iterator(stmts.begin()),std::make_move_iterator(stmts.end()));
}






