//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTDesugar.h"
#include "../Preprocessor/SimpleExprInterpreter.h"

ASTDesugar::ASTDesugar(const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables, const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
                       const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_property_functions, const std::vector<std::unique_ptr<NodeUIControl>> &m_builtin_widgets)
: m_builtin_functions(m_builtin_functions), m_builtin_variables(m_builtin_variables), m_property_functions(m_property_functions), m_builtin_widgets(m_builtin_widgets) {
}

void ASTDesugar::visit(NodeProgram& node) {
    m_function_definitions = std::move(node.function_definitions);

    // check for init callback; get pointer to init callback
	auto it = std::find_if(node.callbacks.begin(), node.callbacks.end(), [](const std::unique_ptr<NodeCallback>& callback) {
	  return callback->begin_callback == "on init";
	});
	if (it == node.callbacks.end()) {
		// Fehlerbehandlung, wenn kein init_callback gefunden wurde
        CompileError(ErrorType::SyntaxError, "Unable to compile. Missing <init callback>.", -1, "", "", node.tok.file).exit();
	} else {
		// Verschieben Sie den gefundenen Callback an die vorderste Stelle
		std::rotate(node.callbacks.begin(), it, std::next(it));
		m_init_callback = node.callbacks[0].get();
	}

    declare_compiler_variables();
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
	evaluating_functions = true;
    for(auto & function : m_function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
    		m_processing_function = true;
            m_variable_scope_stack.emplace();
            function->body->accept(*this);
            m_variable_scope_stack.pop();
        }
    }
	evaluating_functions = false;
    for(auto & function : m_function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            node.function_definitions.push_back(std::move(function));
        }
    }
    // add declarations from function bodies to beginning of m_init_callback
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
    // function args substitution
    if(!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.name)) {
            if(auto substitute_array = cast_node<NodeArray>(substitute.get())) {
                node.name = substitute_array->name;
            } else {
                node.replace_with(std::move(substitute));
                return;
            }
        }
    }

    if(contains(ARRAY_IDENT, node.name[0])) {
        std::string identifier(1, node.name[0]);
        node.name = node.name.erase(0,1);
        token token_type = get_token_type(TYPES, identifier);
        node.type = token_to_type(token_type);
    }
    // local variable substitution
    if(!m_variable_scope_stack.empty()) {
        if(auto substitute = get_local_variable_substitute(node.name)) {
            node.name = substitute->get_string();
        }
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
            node.replace_with(std::move(substitute));
            return;
        }
    }
	// save original type before substitution
	ASTType original_type = Unknown;
    // if notated without brackets -> variable can be array
    if(contains(VAR_IDENT, node.name[0]) || contains(ARRAY_IDENT, node.name[0])) {
        std::string identifier(1, node.name[0]);
        node.name = node.name.erase(0,1);
        token token_type = get_token_type(TYPES, identifier);
        original_type = token_to_type(token_type);
    }
    // local variable substitution
    if(!m_variable_scope_stack.empty()) {
        if(auto substitute = get_local_variable_substitute(node.name)) {
            substitute->update_parents(node.parent);
            if(substitute->type == Unknown)
                substitute->type = original_type;
            node.replace_with(std::move(substitute));
            return;
        }
    }
    // add prefixes
    if(!m_family_prefixes.empty()) {
        // add prefixes only if parent is declare statement and node is to_be_declared and not assigned
        auto node_declare_statement = cast_node<NodeSingleDeclareStatement>(node.parent);
        if(node_declare_statement and &node == node_declare_statement->to_be_declared.get()){
            node.name = m_family_prefixes.top() + "." + node.name;
        }
    }
    if(!m_const_prefixes.empty()) {
        node.name = m_const_prefixes.top() + "." + node.name;
    }
}

void ASTDesugar::visit(NodeFunctionCall& node) {
    if(node.is_call and !node.function->args->params.empty()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect amount of function arguments when using <call>.", node.tok.line, "0", std::to_string(node.function->args->params.size()), node.tok.file).exit();
    }
    if(std::find(m_function_call_stack.begin(), m_function_call_stack.end(), node.function->name) != m_function_call_stack.end()) {
        // recursive function call detected
        CompileError(ErrorType::SyntaxError,"Recursive function call detected. Calling functions inside their definition is not allowed.", node.tok.line, "", node.function->name, node.tok.file).exit();
    }
    node.function->accept(*this);

	if(node.function->name == "set_note_selection") {
		std::cout <<"";
	}
    // substitution start
    if (auto node_function_def = get_function_definition(node.function.get())) {
		if(node.is_call and node_function_def->return_variable.has_value()) {
			CompileError(ErrorType::SyntaxError, "Found incorrect use of return variable when using <call>.", node.tok.line, "", "", node.tok.file).exit();
		}
//        node_function_def->is_used = true;
        if(node.is_call) {
//			node_function_def->body->accept(*this);
//            if(has_local_variables) m_variable_scope_stack.pop();
//            has_local_variables = false;
//			m_function_call_stack.pop_back();
            return;
		}
        // make functions call
        if(node.function->args->params.empty() and !node_function_def->return_variable.has_value()) {
            node.is_call = true;
//			node_function_def->body->accept(*this);
//            if(has_local_variables) m_variable_scope_stack.pop();
//            has_local_variables = false;
//			m_function_call_stack.pop_back();
            return;
        }
		m_function_call_stack.push_back(node.function->name);
        m_variable_scope_stack.emplace();
		node_function_def->parent = node.parent;
		if (!node.function->args->params.empty()) {
			auto substitution_vec = get_substitution_vector(node_function_def->header.get(), node.function.get());
			m_substitution_stack.push(std::move(substitution_vec));
		}
		node_function_def->body->update_token_data(node.tok);
		node_function_def->body->accept(*this);
		if (!node.function->args->params.empty()) m_substitution_stack.pop();
//        pop_substitution_stack();
        m_variable_scope_stack.pop();
//        has_local_variables = false;

		m_function_call_stack.pop_back();
//		m_processing_function = evaluating_functions || !m_function_call_stack.empty();
        // has return variable
        if(node_function_def->return_variable.has_value()) {
            if(is_instance_of<NodeStatementList>(node.parent->parent)) {
                CompileError(ErrorType::SyntaxError,"Function returns a value. Move function into assign statement.", node.tok.line, node_function_def->return_variable.value()->get_string(), "<assignment>", node.tok.file).exit();
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
                        auto node_declare = std::make_unique<NodeSingleDeclareStatement>(node_declaration->to_be_declared->clone(),
                                                                                         nullptr, node_declaration->tok);
                        node_function_def->body->statements.insert(node_function_def->body->statements.begin(),
                                                                   statement_wrapper(std::move(node_declare), node_function_def->body.get()));
                    } else if (auto node_assignment = cast_node<NodeSingleAssignStatement>(node.parent)) {
                        substitution_vec.emplace_back(node_function_def->return_variable.value()->get_string(),
                                                      node_assignment->array_variable->clone());
                    }
                    m_substitution_stack.push(std::move(substitution_vec));
                    node_function_def->body->accept(*this);
                    node_function_def->body->parent = node.parent->parent;
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
    } else if (auto property_func = get_property_function(node.function.get())) {
        if(node.function->args->params.size() < 2)
            CompileError(ErrorType::SyntaxError,"Found Property Function with insufficient amount of arguments.", node.tok.line, "At least 2 arguments", std::to_string(node.function->args->params.size()), node.tok.file).exit();
        auto node_statement_list = inline_property_function(property_func, std::move(node.function));
        node_statement_list->accept(*this);
        node.replace_with(std::move(node_statement_list));
        return;
    } else {
        CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).exit();
    }
}

void ASTDesugar::pop_substitution_stack() {
    if(m_function_call_stack.size() > m_substitution_stack.size())
        m_substitution_stack.pop();
}

std::unique_ptr<NodeStatementList> ASTDesugar::inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header) {
    auto node_statement_list = std::make_unique<NodeStatementList>(function_header->tok);
    for(int i = 1; i<function_header->args->params.size(); i++) {
        auto node_get_control = std::make_unique<NodeGetControlStatement>(function_header->args->params[0]->clone(), property_function->args->params[i]->get_string(), function_header->tok);
        auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_get_control), std::move(function_header->args->params[i]), function_header->tok);
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_assignment), node_statement_list.get()));
    }
    node_statement_list->update_parents(function_header->parent);
    return std::move(node_statement_list);
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
	if(node.to_be_declared->get_string() == "hold_value") {
		std::cout <<"";
	}
    // check if is_persistent
    bool is_persistent = false;
	bool is_local = false;
	bool is_global = false;
    auto node_array = cast_node<NodeArray>(node.to_be_declared.get());
    if(node_array) {is_persistent = node_array->is_persistent; is_local = node_array->is_local; is_global = node_array->is_global;}
    auto node_variable = cast_node<NodeVariable>(node.to_be_declared.get());
    if(node_variable) {is_persistent = node_variable->is_persistent; is_local = node_variable->is_local; is_global = node_variable->is_global;}
	// if currently in function inlining
	if(in_function() and !is_global) {
		node.to_be_declared ->accept(*this);
        auto new_name = "_"+node.to_be_declared->get_string()+std::to_string(local_var_counter++);
		std::unique_ptr<NodeAST> node_substitute;
		if(node_array) {
			auto node_local_array = std::unique_ptr<NodeArray>(static_cast<NodeArray *>(node.to_be_declared->clone().release()));
			node_local_array->name = new_name;
			node_substitute = std::move(node_local_array);
		} else if(node_variable) {
			auto node_local_var = std::unique_ptr<NodeVariable>(static_cast<NodeVariable *>(node.to_be_declared->clone().release()));
			node_local_var->name = new_name;
			node_substitute = std::move(node_local_var);
		}
//        has_local_variables = true;
		m_variable_scope_stack.top().insert({node.to_be_declared->get_string(), std::move(node_substitute)});
	}
    node_variable = cast_node<NodeVariable>(node.to_be_declared.get());
    node_array = cast_node<NodeArray>(node.to_be_declared.get());

	node.to_be_declared ->accept(*this);
    if(node.assignee)
        node.assignee -> accept(*this);
    // in case node.assignee is function substitution -> then this node gets replaced
    if(!node.to_be_declared and !node.assignee) return;

    // add make_persistent and read_persistent_var
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);

    // see if assignee is param_list
    NodeParamList* param_list = nullptr;
    if(node.assignee and node_array) param_list = cast_node<NodeParamList>(node.assignee.get());
    // make param list if it is array declaration and no param list as assignee
    if(node_array and node.assignee and !param_list) {
        auto node_param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.assignee->tok));
        node_param_list->params.push_back(std::move(node.assignee));
        node_param_list->parent = &node;
        node.assignee = std::move(node_param_list);
    }

    if(node_array) {
        // array size is empty -> try to infer from assignee
        if(node_array->sizes->params.empty()) {
            // no assignee -> no array size -> not able to infer
            if(!node.assignee) {
                CompileError(ErrorType::SyntaxError,"Unable to infer array size.", node.tok.line, "initializer list", "",node.tok.file).exit();
            }
            // if size is empty -> get it from declaration
            if(param_list) {
                auto node_int = make_int((int32_t) param_list->params.size(), node_array->sizes.get());
                node_array->sizes->params.push_back(std::move(node_int));
            }
        // not empty ->array size is dimension
        } else {
            node_array->dimensions = node_array->sizes->params.size();
        }
        // multidimensional array
        if (node_array->dimensions > 1) {
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
    } else if(node_variable) {
        // a list of values is assigned to a declared variable
        if(param_list and param_list->params.size() > 1) {
            CompileError(ErrorType::SyntaxError,"Unable to assign a list of values to variable.", node.tok.line, "single value", "list of values",node.tok.file).exit();
        }
    }

    if(is_persistent) {
		NodeAST* to_be_declared_ptr = node.to_be_declared.get();
		// clear indexes if it is array because of: make_persistent_var(arr[124]) <-
		std::unique_ptr<NodeArray> node_array_copy;
		if(node_array) {
			node_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray *>(node.to_be_declared->clone().release()));
			node_array_copy->indexes->params.clear();
			to_be_declared_ptr = node_array_copy.get();
		}
        add_vector_to_statement_list(node_statement_list, add_read_functions(to_be_declared_ptr, node_statement_list.get()));
    }



    // special treatment if declaration is inside function -> move to init_callback
    if(in_function() and !m_in_init_callback) {
        std::unique_ptr<NodeAST> stmt;
        if(node.assignee) {
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(node.to_be_declared->clone(), node.assignee->clone(),node.tok);
            node_assignment->update_parents(node.parent);
			node_assignment->accept(*this);
            stmt = std::move(node_assignment);
        } else {
            auto node_dead_end = std::make_unique<NodeDeadEnd>(node.tok);
            stmt = std::move(node_dead_end);
        }
        // check if this var has already been moved to m_declare_statements_to_move
        auto it = std::find_if(m_declare_statements_to_move.begin(), m_declare_statements_to_move.end(),
                               [&](const std::unique_ptr<NodeStatement> &stmt) {
                                   return stmt->get_string() == node.to_be_declared->get_string();
                               });
        bool local_variable_already_declared = it != m_declare_statements_to_move.end();
        if(!local_variable_already_declared) {
            auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node.to_be_declared),
                                                                                 nullptr, node.tok);
            node_statement_list->statements.insert(node_statement_list->statements.begin(), statement_wrapper(std::move(node_declaration), m_init_callback));
            auto statement = statement_wrapper(std::move(node_statement_list), m_init_callback);
            statement->update_parents(m_init_callback);
            m_declare_statements_to_move.push_back(std::move(statement));
        }
        node.replace_with(std::move(stmt));
    } else {
        node_statement_list->statements.insert(node_statement_list->statements.begin(), statement_wrapper(node.clone(), node_statement_list.get()));
        node_statement_list->update_parents(node.parent);
        node.replace_with(std::move(node_statement_list));
    }

}

bool ASTDesugar::in_function() {
    return !m_function_call_stack.empty() || evaluating_functions;
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

std::vector<std::unique_ptr<NodeStatement>> ASTDesugar::add_read_functions(NodeAST* var, NodeAST* parent) {
    std::vector<std::unique_ptr<NodeStatement>> statements;

    std::string persistent1 = "make_persistent";
    std::string persistent2 = "read_persistent_var";
    auto node_function = get_builtin_function(persistent1)->clone();
    auto make_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    make_persistent->args->params.clear();
    make_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(make_persistent), parent));

    node_function = get_builtin_function(persistent2)->clone();
    auto read_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    read_persistent->args->params.clear();
    read_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(read_persistent), parent));
    return std::move(statements);
}

void ASTDesugar::visit(NodeSingleAssignStatement& node) {
    node.array_variable ->accept(*this);
    // needed to add check, because if node.array_variable is getControlStatement, this whole node will be replaced
    if(node.assignee)
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

void ASTDesugar::visit(NodeGetControlStatement& node) {
    node.ui_id ->accept(*this);

    auto node_assign_statement = cast_node<NodeSingleAssignStatement>(node.parent);
    std::string control_function = "get_control_par";
    if(node_assign_statement && node_assign_statement->array_variable.get() == &node) control_function = "set_control_par";

    auto control_param = shorthand_to_control_param(node.control_param);
    if(!control_param) {
        CompileError(ErrorType::SyntaxError,
                     "Did not recognize control parameter.", node.tok.line, "valid $CONTROL_PAR", node.control_param, node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    ASTType control_function_type = get_control_function_type(node.control_param);
    if(control_function_type == String) control_function += "_str";
    auto node_control_function = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function(control_function)->clone().release()));
    node_control_function->update_token_data(node.tok);
    node_control_function->args->params.clear();
    // if it is a variable -> wrap it in get_ui_id()
    if(is_instance_of<NodeVariable>(node.ui_id.get())) {
        auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function("get_ui_id")->clone().release()));
        node_get_ui_id->args->params.clear();
        node_get_ui_id->args->params.push_back(std::move(node.ui_id));
        node_control_function->args->params.push_back(std::make_unique<NodeFunctionCall>(false, std::move(node_get_ui_id), node.tok));
    } else {
        node_control_function->args->params.push_back(std::move(node.ui_id));
    }
    node_control_function->args->params.push_back(std::move(control_param));
    if(node_assign_statement && node_assign_statement->array_variable.get() == &node) {
        node_assign_statement->assignee->parent = node_control_function->args.get();
        node_control_function->args->params.push_back(std::move(node_assign_statement->assignee));
        node_control_function->args->params[node_control_function->args->params.size()-1]->accept(*this);
    }
    auto node_control_function_call = std::make_unique<NodeFunctionCall>(false, std::move(node_control_function), node.tok);
    node_control_function_call->update_parents(node.parent);
    // if it is in an assignment statement and this node is the left side, replace the parent of this node too
    if(node_assign_statement && node_assign_statement->array_variable.get() == &node) {
        node.parent->replace_with(std::move(node_control_function_call));
    } else {
        node.replace_with(std::move(node_control_function_call));
    }
}

void ASTDesugar::visit(NodeFamilyStatement& node) {
    std::string pref = node.prefix;
    if(!m_family_prefixes.empty()) pref = m_family_prefixes.top() + "." + node.prefix;
    m_family_prefixes.push(pref);
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    for(auto &member : node.members) {
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
        node.statements->statements.push_back(std::move(inc_statement));
    } else {
        auto assign_var2 = function_var->clone();
        auto inc_expression = make_binary_expr(Integer, "+", std::move(function_var), std::move(node.step), &node, node.tok);
        auto node_inc_statement = std::make_unique<NodeSingleAssignStatement>(std::move(assign_var2), std::move(inc_expression), node.tok);
        auto node_statement = statement_wrapper(std::move(node_inc_statement), node_while_statement.get());
        node_statement->update_parents(&node);
        node.statements->statements.push_back(std::move(node_statement));
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

void ASTDesugar::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
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

void ASTDesugar::visit(NodeUIControl &node) {
	auto engine_widget = get_builtin_widget(node.ui_control_type);
	if(!engine_widget) {
		CompileError(ErrorType::SyntaxError, "Did not recognize engine widget.", node.tok.line, "valid widget type", node.ui_control_type, node.tok.file).exit();
	}

	node.control_var->accept(*this);
	node.params->accept(*this);

	auto node_widget_array = cast_node<NodeArray>(engine_widget->control_var.get());
	// check if is_persistent
	bool is_persistent = false;
	auto node_array = cast_node<NodeArray>(node.control_var.get());
	if(node_array) is_persistent = node_array->is_persistent;
	auto node_variable = cast_node<NodeVariable>(node.control_var.get());
	if(node_variable) is_persistent = node_variable->is_persistent;

	// add make_persistent and read_persistent_var
	auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
	// is UI Array
	if(node_array and !node_widget_array) {
		if(node_array->sizes->params.empty()) {
			CompileError(ErrorType::SyntaxError,"Unable to infer array size.", node.tok.line, "initializer list", "",node.tok.file).exit();
		}
		node_array->dimensions = node_array->sizes->params.size();
		// multidimensional array
		auto node_expression = create_right_nested_binary_expr(node_array->sizes->params, 0, "*", node_array->tok);
		// calculate array size
		SimpleInterpreter eval;
		auto array_size = eval.evaluate_int_expression(node_expression);
		if(array_size.is_error()) {
			array_size.get_error().exit();
		}
		auto node_ui_array_declaration = std::make_unique<NodeSingleDeclareStatement>(node_array->clone(), nullptr, node.tok);
		node_ui_array_declaration->to_be_declared->type = engine_widget->control_var->type;
		node_statement_list->statements.push_back(statement_wrapper(node_ui_array_declaration->clone(), node_statement_list.get()));
		std::string new_control_name = node_array->name+std::to_string(0);
		if(node_array->dimensions>1) new_control_name = "_"+new_control_name;
		for(int i = 0; i<array_size.unwrap(); i++) {
			new_control_name = node_array->name+std::to_string(i);
			if(node_array->dimensions>1) new_control_name = "_"+new_control_name;
			auto node_control_var = std::make_unique<NodeVariable>(false, new_control_name, UI_Control, node.tok);
			auto new_node_ui_control = std::unique_ptr<NodeUIControl>(static_cast<NodeUIControl*>(node.clone().release()));
			new_node_ui_control->control_var = node_control_var->clone();
			auto new_node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(new_node_ui_control),
																					 nullptr, node.tok);
			node_statement_list->statements.push_back(statement_wrapper(std::move(new_node_declaration), node_statement_list.get()));
			if(is_persistent) add_vector_to_statement_list(node_statement_list, add_read_functions(node_control_var.get(), node_statement_list.get()));
		}

		auto node_iterator_var = std::make_unique<NodeVariable>(false, m_compiler_variables[1], VarType::Mutable, node.tok);
//		node_iterator_var->accept(*this);
		auto node_while_body = std::make_unique<NodeStatementList>(node.tok);

		auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function("get_ui_id")->clone().release()));
		node_get_ui_id->args->params.clear();
		node_get_ui_id->args->params.push_back(std::make_unique<NodeVariable>(node_array->is_persistent, new_control_name, UI_Control, node.tok));

		auto node_while_body_expression = make_binary_expr(ASTType::Integer, "+", std::move(node_get_ui_id), node_iterator_var->clone(),nullptr, node.tok);

		auto node_raw_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(node_array->clone().release()));
		node_raw_array_copy->indexes->params.clear();
		node_raw_array_copy->indexes->params.push_back(node_iterator_var->clone());
		// only access raw array if multidimensional
		if(node_array->dimensions>1) node_raw_array_copy->name = "_"+node_raw_array_copy->name;
		auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_raw_array_copy), std::move(node_while_body_expression), node.tok);
		node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
		auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, array_size.unwrap(), std::move(node_while_body), node_statement_list.get());
		node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_loop), node_statement_list.get()));
		node_statement_list->update_parents(node.parent);
		node_statement_list->accept(*this);

	} else {
		if(is_persistent) {
			add_vector_to_statement_list(node_statement_list, add_read_functions(node.control_var.get(), node_statement_list.get()));
		}
		// split declare statement into declare + assign statement
		auto node_declare_statement = std::unique_ptr<NodeSingleDeclareStatement>(static_cast<NodeSingleDeclareStatement*>(node.parent->clone().release()));
		if(node_declare_statement->assignee) {
			auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(std::move(node.control_var), std::move(node_declare_statement->assignee), node.tok);
			node_statement_list->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_statement_list.get()));
		}
		node_statement_list->statements.insert(node_statement_list->statements.begin(), statement_wrapper(std::move(node_declare_statement), node_statement_list.get()));
		node_statement_list->update_parents(node.parent->parent);
	}
	node.parent->replace_with(std::move(node_statement_list));
}

void ASTDesugar::visit(NodeListStatement &node) {
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    auto node_main_array = make_array(node.name, node.size, node.tok, node_statement_list.get());
    // accept first to get rid of array identifier
    node_main_array->accept(*this);
    node_main_array->var_type = List;
    auto node_declare_main_array = std::make_unique<NodeSingleDeclareStatement>(node_main_array->clone(), nullptr, node.tok);
    auto main_size = (int32_t)node.body.size();
    auto node_declare_main_const = std::make_unique<NodeSingleDeclareStatement>(std::make_unique<NodeVariable>(false, node_main_array->name+".SIZE", VarType::Const, node.tok), make_int(main_size,&node), node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_array), node_statement_list.get()));
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_const), node_statement_list.get()));

    auto node_sizes_array = make_array(node_main_array->name+".sizes", main_size, node.tok, nullptr);
    auto node_positions_array = make_array(node_main_array->name+".pos", main_size, node.tok, nullptr);
    std::vector<int32_t> sizes(node.body.size());
    std::vector<int32_t> positions(node.body.size());
    auto node_sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
    auto node_positions = std::unique_ptr<NodeParamList>(new NodeParamList({}, node.tok));
    positions[0] = 0;
    for(int i = 0; i<node.body.size(); i++) {
        sizes[i] = static_cast<int32_t>(node.body[i]->params.size());
        if(i>0) positions[i] = positions[i - 1] + sizes[i - 1];
//        std::cout << sizes[i] << ", " << positions[i] << std::endl;
        auto node_size = make_int(sizes[i], node_sizes.get());
        node_sizes->params.push_back(std::move(node_size));
        auto node_position = make_int(positions[i], node_positions.get());
        node_positions->params.push_back(std::move(node_position));
    }
    auto node_sizes_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_sizes_array), std::move(node_sizes), node.tok);
    auto node_positions_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_positions_array), std::move(node_positions), node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_sizes_declaration), node_statement_list.get()));
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_positions_declaration), node_statement_list.get()));

    auto node_iterator_var = std::make_unique<NodeVariable>(false, m_compiler_variables[0], VarType::Mutable, node.tok);
    for(int i = 0; i<node.body.size(); i++) {
        auto node_array_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        auto node_array = make_array(node_main_array->name+std::to_string(i), sizes[i], node.tok, node_array_declaration.get());
        node_array_declaration->to_be_declared = node_array->clone();
        node_array_declaration->assignee = std::move(node.body[i]);
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_array_declaration), node_statement_list.get()));

        auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        auto node_variable = std::make_unique<NodeVariable>(false, node_main_array->name+std::to_string(i)+".SIZE", VarType::Const, node.tok);
        node_const_declaration->to_be_declared = std::move(node_variable);
        node_const_declaration->assignee = make_int(sizes[i], node_const_declaration.get());
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_const_declaration), node_statement_list.get()));

        auto node_while_body = std::make_unique<NodeStatementList>(node.tok);
        auto node_expression = make_binary_expr(ASTType::Integer, "+", node_iterator_var->clone(), make_int(positions[i], &node),nullptr, node.tok);
        node_main_array->indexes->params.clear();
        node_main_array->indexes->params.push_back(std::move(node_expression));

        node_array->indexes->params.push_back(node_iterator_var->clone());
        auto node_main_array_copy = std::unique_ptr<NodeArray>(static_cast<NodeArray*>(node_main_array->clone().release()));
        node_main_array_copy->name = "_"+node_main_array_copy->name;
        auto node_assignment = std::make_unique<NodeSingleAssignStatement>(std::move(node_main_array_copy), std::move(node_array), node.tok);
        node_while_body->statements.push_back(statement_wrapper(std::move(node_assignment), node_while_body.get()));
        auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_statement_list.get());
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_loop), node_statement_list.get()));
    }
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);
    node.replace_with(std::move(node_statement_list));
}

void ASTDesugar::declare_compiler_variables() {
    for(auto & var_name: m_compiler_variables) {
        Token tok = Token(KEYWORD, var_name, 0, (std::string&)"");
        auto node_variable = std::make_unique<NodeVariable>(false, var_name, VarType::Mutable, tok);
        auto node_var_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_variable), nullptr, tok);
        node_var_declaration->to_be_declared->parent = node_var_declaration.get();
        node_var_declaration->accept(*this);
        m_declare_statements_to_move.push_back(statement_wrapper(std::move(node_var_declaration), m_init_callback->statements.get()));
    }
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

NodeFunctionHeader* ASTDesugar::get_property_function(NodeFunctionHeader *function) {
    auto it = std::find_if(m_property_functions.begin(), m_property_functions.end(),
                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
                               return func->name == function->name;
                           });
    if(it != m_property_functions.end()) {
        return m_property_functions[std::distance(m_property_functions.begin(), it)].get();
    }
    return nullptr;
}

NodeUIControl* ASTDesugar::get_builtin_widget(const std::string &ui_control) {
	auto it = std::find_if(m_builtin_widgets.begin(), m_builtin_widgets.end(),
						   [&](const std::unique_ptr<NodeUIControl> &widget) {
							 return (widget->ui_control_type == ui_control);
						   });
	if(it != m_builtin_widgets.end()) {
		return m_builtin_widgets[std::distance(m_builtin_widgets.begin(), it)].get();
	}
	return nullptr;
}

ASTType ASTDesugar::get_control_function_type(const std::string& control_param) {
    std::string control_par = to_lower(control_param);
    std::vector<std::string> str_substrings{"name", "path", "picture", "help", "identifier", "label", "text"};
    std::vector<std::string> int_substrings{"state", "alignment", "pos", "shifting"};
    ASTType type = Integer;
    for (auto const &substring : str_substrings) {
        if(contains(control_par, substring)) {
            type = ASTType::String;
            break;
        }
    }
    for (auto const &substring : int_substrings) {
        if(contains(control_par, substring)) {
            type = Integer;
            break;
        }
    }
    return type;
}

std::unique_ptr<NodeVariable> ASTDesugar::shorthand_to_control_param(const std::string& shorthand) {
    std::string control_par = to_lower(shorthand);
    if(control_par == "x") control_par = "pos_x";
    if(control_par == "y") control_par = "pos_y";
    if(control_par == "default") control_par += "_value";
    auto it = std::find_if(m_builtin_variables.begin(), m_builtin_variables.end(),
                           [&](const std::unique_ptr<NodeVariable> &var) {
                               return string_compare(control_par, var->name) or string_compare("control_par_"+control_par, var->name);
                           });
    if(it != m_builtin_variables.end()) {
        auto control_var = m_builtin_variables[std::distance(m_builtin_variables.begin(), it)]->clone();
        return std::unique_ptr<NodeVariable>(static_cast<NodeVariable*>(control_var.release()));
    }
    return nullptr;
}

std::unique_ptr<NodeAST> ASTDesugar::get_local_variable_substitute(const std::string& name) {
    auto it = m_variable_scope_stack.top().find(name);
    if(it != m_variable_scope_stack.top().end()) {
        return it->second->clone();
    }
    return nullptr;
}







