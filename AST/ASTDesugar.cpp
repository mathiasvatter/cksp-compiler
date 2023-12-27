//
// Created by Mathias Vatter on 27.10.23.
//
#include <functional>

#include "ASTDesugar.h"
#include "../Preprocessor/SimpleExprInterpreter.h"

ASTDesugar::ASTDesugar(const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &m_builtin_variables,
                       const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions,
                       const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &m_property_functions,
                       const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets)
: m_builtin_functions(m_builtin_functions), m_builtin_variables(m_builtin_variables), m_property_functions(m_property_functions), m_builtin_widgets(m_builtin_widgets) {
}

void ASTDesugar::visit(NodeProgram& node) {
    m_program = &node;
    for(auto & def : node.function_definitions) {
        m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
    }

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
    declare_dummy_return_variable();
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    // declare after callback visiting because of size of m_local_variables
    declare_compiler_variables();
	evaluating_functions = true;
    for(auto & function : m_function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            m_functions_in_use.insert({function->header->name, function.get()});
            m_variable_scope_stack.emplace_back();
            function->body->accept(*this);
            m_variable_scope_stack.pop_back();
            m_functions_in_use.erase(function->header->name);
        }
    }
	evaluating_functions = false;
    // add local variables from function bodies to beginning of m_init_callback
    m_init_callback->statements->statements.insert(m_init_callback->statements->statements.begin(),
                                                   std::make_move_iterator(m_local_declare_statements.begin()),
                                                   std::make_move_iterator(m_local_declare_statements.end()));
}

void ASTDesugar::visit(NodeCallback& node) {
    m_variable_scope_stack.emplace_back();
    // empty the local var stack after init, because the idx can be reused now
    if(m_current_callback == m_init_callback) {
        m_local_variables = std::stack<std::string>();
    }
    m_current_callback = &node;
    if(node.callback_id)
        node.callback_id->accept(*this);
    node.statements->accept(*this);
    m_current_callback_idx++;

    m_variable_scope_stack.pop_back();
}

void ASTDesugar::visit(NodeBinaryExpr& node) {
	node.left->accept(*this);
	node.right->accept(*this);

    auto right_int = cast_node<NodeInt>(node.right.get());
    auto left_int = cast_node<NodeInt>(node.left.get());
    auto right_real = cast_node<NodeReal>(node.right.get());
    auto left_real = cast_node<NodeReal>(node.left.get());

	if(get_token_type(MATH_OPERATORS, node.op) or get_token_type(BITWISE_OPERATORS, node.op)) {
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
//                {DIV, [](int32_t a, int32_t b) { return a / b; }},
                {MODULO, [](int32_t a, int32_t b) { return a % b; }},
                {BIT_AND, [](int32_t a, int32_t b) { return a & b; }},
                {BIT_OR, [](int32_t a, int32_t b) { return a | b; }},
                {BIT_XOR, [](int32_t a, int32_t b) { return a ^ b; }}
            };
            token tok = *get_token_type(ALL_OPERATORS, node.op);
            if (int_operations.find(tok) != int_operations.end()) {
                result = int_operations[tok](left_int->value, right_int->value);
                auto new_node = std::make_unique<NodeInt>(result, node.tok);
                new_node->parent = node.parent;
                node.replace_with(std::move(new_node));
            }
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
                {ADD, [](double a, double b) { return a + b; }},
                {SUB, [](double a, double b) { return a - b; }},
                {MULT, [](double a, double b) { return a * b; }},
                {DIV, [](double a, double b) { return a / b; }},
                {MODULO, [](double a, double b) { return std::fmod(a, b); }}
            };
            token tok = *get_token_type(MATH_OPERATORS, node.op);
            if (real_operations.find(tok) != real_operations.end()) {
                result = real_operations[tok](left_real->value, right_real->value);
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
            if(is_instance_of<NodeArray>(substitute.get()) || is_instance_of<NodeVariable>(substitute.get())) {
                node.name = substitute->get_string();
            } else {
                node.replace_with(std::move(substitute));
                return;
            }
        }
    }

    if(contains(VAR_IDENT, node.name[0]) || contains(ARRAY_IDENT, node.name[0])) {
        std::string identifier(1, node.name[0]);
        node.name = node.name.erase(0,1);
        token token_type = *get_token_type(TYPES, identifier);
        node.type = token_to_type(token_type);
    }
    // local variable substitution
    // do local variable substitution only if parent is not declare statement because scope
    if(!m_variable_scope_stack.empty() and !is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
        if(auto substitute = get_local_variable_substitute(node.name)) {
            node.name = substitute->get_string();
            node.is_local = true;
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
        token token_type = *get_token_type(TYPES, identifier);
        node.type = token_to_type(token_type);
        original_type = node.type;
    }
    // local variable substitution
    // do local variable substitution only if parent is not declare statement because scope
    if(!m_variable_scope_stack.empty() and !is_instance_of<NodeSingleDeclareStatement>(node.parent)) {
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
    if(m_functions_in_use.find(node.function->name) != m_functions_in_use.end()) {
        // recursive function call detected
        CompileError(ErrorType::SyntaxError,"Recursive function call detected. Calling functions inside their definition is not allowed.", node.tok.line, "", node.function->name, node.tok.file).exit();
    }
    NodeAST* old_function_inline_statement = m_current_function_inline_statement;
    NodeStatement* nearest_statement = static_cast<NodeStatement*>(m_current_function_inline_statement);
    node.function->accept(*this);

    // substitution start
    if (auto function_def = get_function_definition(node.function.get())) {
        if(evaluating_functions and m_current_callback != m_init_callback and function_def->header->args->params.empty() and !function_def->return_variable.has_value())
            m_function_call_order.push_back(function_def);
		if(node.is_call and function_def->return_variable.has_value()) {
			CompileError(ErrorType::SyntaxError, "Found incorrect use of return variable when using <call>.", node.tok.line, "", "", node.tok.file).exit();
		}
        function_def->call.insert(&node);
        m_current_function = function_def;
        // inline if current call is <on init>
        if(m_current_callback != m_init_callback) {
            if (node.is_call) {
                if(function_def->is_compiled) {
                    return;
                } else {
                    function_def->is_compiled = true;
                }
            }

        } else if(node.is_call){
            CompileError(ErrorType::SyntaxError, "The usage of <call> keyword is not allowed in the <on init> callback. Automatically removed <call> and inlined function. Consider not using the <call> keyword.", node.tok.line, "", "<call>", node.tok.file).print();
            node.is_call = false;
        }

        auto node_function_def = std::unique_ptr<NodeFunctionDefinition>(static_cast<NodeFunctionDefinition*>(function_def->clone().release()));
        node_function_def->update_parents(nullptr);
		m_function_call_stack.push(node.function->name);
        m_functions_in_use.insert({node.function->name, function_def});
        m_variable_scope_stack.emplace_back();
		node_function_def->parent = node.parent;
		if (!node.function->args->params.empty()) {
			auto substitution_map = get_substitution_map(node_function_def->header.get(), node.function.get());
			m_substitution_stack.push(std::move(substitution_map));
		}
		node_function_def->body->update_token_data(node.tok);
		node_function_def->body->accept(*this);
		if (!node.function->args->params.empty()) m_substitution_stack.pop();
        m_variable_scope_stack.pop_back();
        m_functions_in_use.erase(node.function->name);
		m_function_call_stack.pop();
        function_def->call.erase(&node);
        // has return variable
        if(node_function_def->return_variable.has_value()) {
            if(is_instance_of<NodeStatementList>(node.parent->parent)) {
                CompileError(ErrorType::SyntaxError,"Function returns a value. Move function into assign statement.", node.tok.line, node_function_def->return_variable.value()->get_string(), "<assignment>", node.tok.file).exit();
            }
            if (!node_function_def->body->statements.empty()) {
                auto node_assignment = cast_node<NodeSingleAssignStatement>(node_function_def->body->statements[0]->statement.get());
                // strict inlining method only if function body is assign statement
                if (node_function_def->body->statements.size() == 1 and node_assignment) {
                    if (node_assignment->array_variable->get_string() == node_function_def->return_variable.value()->get_string()) {
                        node.replace_with(std::move(node_assignment->assignee));
                        return;
                    } else {
                        CompileError(ErrorType::SyntaxError,
                                     "Given return variable of function could not be found in function body.",
                                     node.tok.line, node_function_def->return_variable.value()->get_string(),
                                     node_assignment->array_variable->get_string(), node.tok.file).exit();
                    }
                // new inlining method
                } else {
                    std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_map;
                    auto node_return_var_name =
                            "_return_var_" + std::to_string(nearest_statement->function_inlines.size());
                    auto node_return_var = std::make_unique<NodeVariable>(false, node_return_var_name, Mutable,
                                                                          node.tok);
                    node_return_var->declaration = m_return_dummy_declaration;
                    node_return_var->is_compiler_return = true;
                    node_return_var->parent = node.parent;
                    substitution_map.insert({node_function_def->return_variable.value()->get_string(),
                                             node_return_var->clone()});
                    auto node_parent_parent = node.parent->parent;
                    node.replace_with(std::move(node_return_var));
                    m_substitution_stack.push(std::move(substitution_map));
                    node_function_def->body->accept(*this);
                    m_substitution_stack.pop();
                    node_function_def->body->parent = node_parent_parent;
                    auto node_statement = statement_wrapper(std::move(node_function_def->body), nullptr);
                    auto ptr = node_statement->statement.get();
                    m_function_inlines.insert({ptr, std::move(node_statement)});
                    nearest_statement->function_inlines.push_back(ptr);
                    m_current_function_inline_statement = old_function_inline_statement;
                    return;
                }
            }
        }
        // inline replacement if not call
        if(!node.is_call) {
            // Only replace if function has actually statements
            if (!node_function_def->body->statements.empty()) {
                node.replace_with(std::move(node_function_def->body));
            } else {
                node.replace_with(std::make_unique<NodeDeadEnd>(node.tok));
            }
        } else {
            m_program->function_definitions.push_back(std::move(node_function_def));
        }
    } else if (auto builtin_func = get_builtin_function(node.function.get())) {
        node.function->type = builtin_func->type;
        node.function->has_forced_parenth = builtin_func->has_forced_parenth;
        node.function->arg_ast_types = builtin_func->arg_ast_types;
        node.function->arg_var_types = builtin_func->arg_var_types;

        if(m_restricted_builtin_functions.find(builtin_func->name) != m_restricted_builtin_functions.end()) {
            if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
                CompileError(ErrorType::SyntaxError,"<"+builtin_func->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.", node.tok.line, "", "<"+m_current_callback->begin_callback+">", node.tok.file).print();
            } else {
                if(m_current_function)
                if(m_current_function->call.find(&node) != m_current_function->call.end())
                    CompileError(ErrorType::SyntaxError,"<"+builtin_func->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks. Not in a called function.", node.tok.line, "", "<"+m_current_function->header->name+"> in <"+m_current_callback->begin_callback+">", node.tok.file).print();
            }
        }

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

void ASTDesugar::visit(NodeStatement& node) {
//    m_current_function_inline_statement = &node;
    node.statement->accept(*this);
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


std::unordered_map<std::string, std::unique_ptr<NodeAST>> ASTDesugar::get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call) {
    std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_vector;
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
        substitution_vector.insert(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<NodeAST> ASTDesugar::get_substitute(const std::string& name) {
    const auto & map = m_substitution_stack.top();
    auto it = map.find(name);
    if(it != map.end()) {
        return it->second->clone();
    }
    return nullptr;
}


void ASTDesugar::visit(NodeSingleDeclareStatement& node) {
    m_current_function_inline_statement = node.parent;
    // check if is_persistent
    bool is_persistent = false;
	bool is_local = false;
	bool is_global = false;
    bool is_const = false;
    bool is_engine = false;
    auto node_array = cast_node<NodeArray>(node.to_be_declared.get());
    if(node_array) {
        if(in_function()) node_array->is_local = true;
        if(m_current_callback != m_init_callback) node_array->is_local = true;
        is_persistent = node_array->is_persistent;
        is_local = node_array->is_local;
        is_global = node_array->is_global;
        if(node_array->is_global) node_array->is_local = false;
        is_const = node_array->var_type == Const;
        is_engine = node_array->is_engine;
    }
    auto node_variable = cast_node<NodeVariable>(node.to_be_declared.get());
    if(node_variable) {
        if(in_function()) node_variable->is_local = true;
        if(m_current_callback != m_init_callback) node_variable->is_local = true;
        is_persistent = node_variable->is_persistent;
        is_local = node_variable->is_local;
        is_global = node_variable->is_global;
        if(node_variable->is_global) node_variable->is_local = false;
        is_const = node_variable->var_type == Const;
        is_engine = node_variable->is_engine;
    }
//    is_local &= !is_global;
    if(is_const and !node.assignee)
        CompileError(ErrorType::SyntaxError,
                     "Found <const> declaration without value assignment.", node.tok.line, "<assignment>", "",node.tok.file).exit();
	// if currently in function inlining
	if(is_local and !is_global and !is_const and !is_engine) {

		node.to_be_declared ->accept(*this);
        auto new_name = "_"+node.to_be_declared->get_string()+"_"+std::to_string(m_local_variables.size());
		std::unique_ptr<NodeAST> node_substitute;
		if(node_array) {
			auto node_local_array = std::unique_ptr<NodeArray>(static_cast<NodeArray *>(node.to_be_declared->clone().release()));
            node_local_array->is_local = true;
			node_local_array->name = new_name;
			node_substitute = std::move(node_local_array);
		} else if(node_variable) {
			auto node_local_var = std::unique_ptr<NodeVariable>(static_cast<NodeVariable *>(node.to_be_declared->clone().release()));
            node_local_var->is_local = true;
			node_local_var->name = "_local_dummy_"+std::to_string(m_local_variables.size());
            node_local_var->declaration = m_local_var_dummy_declaration;
			node_substitute = std::move(node_local_var);
		}
//        if(m_local_variables.find(new_name) == m_local_variables.end()) {
//            m_local_variables.insert(new_name);
//        }
        m_local_variables.push(new_name);
        if(m_variable_scope_stack.back().find(node.to_be_declared->get_string()) != m_variable_scope_stack.back().end()) {
            CompileError(ErrorType::SyntaxError,"Local Variable was already declared in this scope", node.tok.line, "", node.to_be_declared->get_string(),node.tok.file).exit();
        }
		m_variable_scope_stack.back().insert({node.to_be_declared->get_string(), std::move(node_substitute)});
	}
    node_variable = cast_node<NodeVariable>(node.to_be_declared.get());
    node_array = cast_node<NodeArray>(node.to_be_declared.get());

	node.to_be_declared ->accept(*this);

    // in case node.assignee is function substitution -> then this node gets replaced
    if(&node == m_current_node_replaced) {
        m_current_node_replaced = nullptr;
        return;
    }

    if(node.assignee)
        node.assignee -> accept(*this);

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
//            node_array->name = "_"+node_array->name;
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
    if(is_local and !is_engine) {
        std::unique_ptr<NodeAST> stmt;
        std::unique_ptr<NodeAST> assignee = nullptr;
        if (node.assignee and !is_const and !node_array) {
            auto node_assignment = std::make_unique<NodeSingleAssignStatement>(node.to_be_declared->clone(),
                                                                               node.assignee->clone(), node.tok);
            node_assignment->update_parents(node.parent);
            node_assignment->accept(*this);
            stmt = std::move(node_assignment);
        } else {
            assignee = std::move(node.assignee);
            auto node_dead_end = std::make_unique<NodeDeadEnd>(node.tok);
            stmt = std::move(node_dead_end);
        }
        if(node_array or is_global or is_const) {
            std::string var_name_hash = node.to_be_declared->get_string();
            // check if this var has already been moved to m_declare_statements_to_move
            auto it = m_local_already_declared_vars.find(var_name_hash);
            bool local_array_already_declared = it != m_local_already_declared_vars.end();
            if (!local_array_already_declared) {
                auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node.to_be_declared),
                                                                                     std::move(assignee), node.tok);
                node_statement_list->statements.insert(node_statement_list->statements.begin(),
                                                       statement_wrapper(std::move(node_declaration), m_init_callback));
                auto statement = statement_wrapper(std::move(node_statement_list), m_init_callback);
                statement->update_parents(m_init_callback);
                m_local_declare_statements.push_back(std::move(statement));
                m_local_already_declared_vars.insert(var_name_hash);
            }
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
    auto node_function = get_builtin_function(persistent1,1)->clone();
    auto make_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    make_persistent->args->params.clear();
    make_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(make_persistent), parent));

    node_function = get_builtin_function(persistent2,1)->clone();
    auto read_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
    read_persistent->args->params.clear();
    read_persistent->args->params.push_back(var->clone());
    statements.push_back(statement_wrapper(std::move(read_persistent), parent));
    return std::move(statements);
}

void ASTDesugar::visit(NodeSingleAssignStatement& node) {
    m_current_function_inline_statement = node.parent;
    node.array_variable ->accept(*this);

    // needed to add check, because if node.array_variable is getControlStatement, this whole node will be replaced
    if(&node == m_current_node_replaced) {
        m_current_node_replaced = nullptr;
        return;
    }

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
    int function_args = 2;
    if(node_assign_statement && node_assign_statement->array_variable.get() == &node) {
        control_function = "set_control_par";
        function_args = 3;
    }

    auto control_param = shorthand_to_control_param(node.control_param);
    if(!control_param) {
        CompileError(ErrorType::SyntaxError,
                     "Did not recognize control parameter.", node.tok.line, "valid $CONTROL_PAR", node.control_param, node.tok.file).print();
        exit(EXIT_FAILURE);
    }
    ASTType control_function_type = get_control_function_type(node.control_param);
    if(control_function_type == String) control_function += "_str";
    auto node_control_function = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function(control_function, function_args)->clone().release()));
    node_control_function->update_token_data(node.tok);
    node_control_function->args->params.clear();
    // if it is a variable and not builtin -> wrap it in get_ui_id()
    if(is_instance_of<NodeVariable>(node.ui_id.get()) and m_builtin_variables.find(node.ui_id->get_string()) == m_builtin_variables.end()) {
        auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function("get_ui_id",1)->clone().release()));
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
        m_current_node_replaced = node.parent;
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
    m_variable_scope_stack.emplace_back();

	// check if there is only one var and assignee
	if(node.iterator->array_variable->params.size() != 1 or node.iterator->assignee->params.size() != 1) {
		CompileError(ErrorType::SyntaxError, "Found incorrect for-loop syntax.", node.tok.line, "one iterator per for-loop", "multiple iterators", node.tok.file).print();
		exit(EXIT_FAILURE);
	}
    m_current_function_inline_statement = node.parent;
	node.iterator_end->accept(*this);
    node.iterator->array_variable->accept(*this);
    m_current_function_inline_statement = node.parent;
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

    // new local iterator
//    auto node_local_declare_statement = std::make_unique<NodeSingleDeclareStatement>(node.tok);
//    auto node_local_iterator = std::make_unique<NodeVariable>(false, "loc_i", Mutable, node.tok);
//    node_local_iterator->is_local = true;
//    node_local_declare_statement->to_be_declared = std::move(node_local_iterator);
//    node_local_declare_statement->assignee = std::move(node.iterator->assignee->params[0]);

    auto node_assign_statement = std::make_unique<NodeSingleAssignStatement>(node.tok);
    node_assign_statement->array_variable = std::move(assign_var);
    node_assign_statement->assignee = std::move(node.iterator->assignee->params[0]);

    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_statement_list.get()));
//    node_statement_list->statements.push_back(statement_wrapper(std::move(node_local_declare_statement), node_statement_list.get()));

    node_statement_list->statements.push_back(statement_wrapper(std::move(node_while_statement), node_statement_list.get()));
    node_statement_list->update_parents(node.parent);
    node_statement_list->accept(*this);

    m_variable_scope_stack.pop_back();

	node.replace_with(std::move(node_statement_list));
}

void ASTDesugar::visit(NodeWhileStatement& node) {
    m_current_function_inline_statement = node.parent;
    node.condition->accept(*this);
    node.statements->accept(*this);
}

void ASTDesugar::visit(NodeIfStatement& node) {
    m_current_function_inline_statement = node.parent;
    node.condition->accept(*this);
    node.statements->accept(*this);
    node.else_statements->accept(*this);
};

void ASTDesugar::visit(NodeStatementList& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
//    for(int i=0; i<node.statements.size(); ++i) {
//        if(auto node_statement_list = cast_node<NodeStatementList>(node.statements[i]->statement.get())) {
//
//            // Wir speichern die Statements der inneren NodeStatementList
//            auto& inner_statements = node_statement_list->statements;
//
//            inner_statements[0]->function_inlines.insert(inner_statements[0]->function_inlines.end(),
//                                                         node.statements[i]->function_inlines.begin(),
//                                                         node.statements[i]->function_inlines.end());
//            for (auto& stmt : inner_statements) {
//                stmt->parent = &node;
//            }
//            // Fügen Sie die inneren Statements an der aktuellen Position ein
//            node.statements.insert(
//                    node.statements.begin() + i + 1,
//                    std::make_move_iterator(inner_statements.begin()),
//                    std::make_move_iterator(inner_statements.end())
//            );
//            // Entfernen Sie das ursprüngliche NodeStatementList-Element
//            node.statements.erase(node.statements.begin() + i);
//            // Anpassen des Indexes, um die eingefügten Elemente zu berücksichtigen
//            i += inner_statements.size() - 1;
//            // Die inneren Statements sind jetzt leer, da sie verschoben wurden
//            inner_statements.clear();
//        }
//    }
    std::vector<std::unique_ptr<NodeStatement>> temp;
    for(int i = 0; i < node.statements.size(); ++i) {
        if(auto node_statement_list = cast_node<NodeStatementList>(node.statements[i]->statement.get())) {
            // Übertragen Sie die function_inlines vom aktuellen NodeStatementList-Element
            // auf das erste Element der inneren NodeStatementList
            auto& inner_statements = node_statement_list->statements;
            if (!inner_statements.empty()) {
                inner_statements[0]->function_inlines.insert(
                        inner_statements[0]->function_inlines.end(),
                        std::make_move_iterator(node.statements[i]->function_inlines.begin()),
                        std::make_move_iterator(node.statements[i]->function_inlines.end())
                );
            }
            // Aktualisieren Sie das parent-Attribut für jedes innere Statement
            for (auto& stmt : inner_statements) {
                stmt->parent = &node;
            }
            // Fügen Sie die inneren Statements zum temporären Vector hinzu
            temp.insert(
                    temp.end(),
                    std::make_move_iterator(inner_statements.begin()),
                    std::make_move_iterator(inner_statements.end())
            );
            // Überspringen Sie das Hinzufügen des aktuellen NodeStatementList-Elements zu `temp`
            continue;
        }
        // Fügen Sie das aktuelle Element zum temporären Vector hinzu, wenn es nicht speziell behandelt wird
        temp.push_back(std::move(node.statements[i]));
    }
    // Ersetzen Sie die alte Liste durch die neue
    node.statements = std::move(temp);

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
        std::string new_control_name;
		for(int i = 0; i<array_size.unwrap(); i++) {
			new_control_name = node_array->name+std::to_string(i);
			if(node_array->dimensions>1) new_control_name = "_"+new_control_name;
			auto node_control_var = std::make_unique<NodeVariable>(false, new_control_name, UI_Control, node.tok);
            node_control_var->is_used = true;
			auto new_node_ui_control = std::unique_ptr<NodeUIControl>(static_cast<NodeUIControl*>(node.clone().release()));
			new_node_ui_control->control_var = node_control_var->clone();
			auto new_node_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(new_node_ui_control),
																					 nullptr, node.tok);
			node_statement_list->statements.push_back(statement_wrapper(std::move(new_node_declaration), node_statement_list.get()));
			if(is_persistent) add_vector_to_statement_list(node_statement_list, add_read_functions(node_control_var.get(), node_statement_list.get()));
		}
        // reinstantiate control name for while loop after
		new_control_name = node_array->name+std::to_string(0);
		if(node_array->dimensions>1) new_control_name = "_"+new_control_name;

		auto node_iterator_var = std::make_unique<NodeVariable>(false, m_compiler_variables[1], VarType::Mutable, node.tok);
//		node_iterator_var->accept(*this);
		auto node_while_body = std::make_unique<NodeStatementList>(node.tok);

		auto node_get_ui_id = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(get_builtin_function("get_ui_id",1)->clone().release()));
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
    m_current_node_replaced = node.parent;
	node.parent->replace_with(std::move(node_statement_list));
}

void ASTDesugar::visit(NodeListStatement &node) {
    auto node_statement_list = std::make_unique<NodeStatementList>(node.tok);
    auto node_main_array = make_array(node.name, node.size, node.tok, node_statement_list.get());
    // accept first to get rid of array identifier
    node_main_array->accept(*this);
    std::string name_wo_ident = node_main_array->name;
//    node_main_array->name = "_"+node_main_array->name;
    //check dimension -> if only 1 then treat as an array
    int max_dimension = 0;
    for(auto & param : node.body) {
        max_dimension = std::max(max_dimension, (int)param->params.size());
    }
    if(max_dimension>1) node_main_array->var_type = List;

    auto node_declare_main_array = std::make_unique<NodeSingleDeclareStatement>(node_main_array->clone(), nullptr, node.tok);
    auto main_size = (int32_t)node.body.size();
    auto node_declare_main_const = std::make_unique<NodeSingleDeclareStatement>(std::make_unique<NodeVariable>(false, name_wo_ident+".SIZE", VarType::Const, node.tok), make_int(main_size,&node), node.tok);
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_array), node_statement_list.get()));
    node_statement_list->statements.push_back(statement_wrapper(std::move(node_declare_main_const), node_statement_list.get()));


    if(max_dimension == 1) {
        // bring all one sized param lists into the first
        for(int i = 1; i<node.body.size(); i++) {
            node.body[0]->params.push_back(std::move(node.body[i]->params[0]));
        }
        add_vector_to_statement_list(node_statement_list, std::move(array_initialization(node_main_array.get(), node.body[0].get())->statements));
        node_statement_list->update_parents(node.parent);
        node_statement_list->accept(*this);
        node.replace_with(std::move(node_statement_list));
        return;
    }

    auto node_sizes_array = make_array(name_wo_ident+".sizes", main_size, node.tok, nullptr);
    auto node_positions_array = make_array(name_wo_ident+".pos", main_size, node.tok, nullptr);
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
        auto node_array = make_array(name_wo_ident+std::to_string(i), sizes[i], node.tok, node_array_declaration.get());
        node_array_declaration->to_be_declared = node_array->clone();
        node_array_declaration->assignee = std::move(node.body[i]);
        node_statement_list->statements.push_back(statement_wrapper(std::move(node_array_declaration), node_statement_list.get()));

        auto node_const_declaration = std::make_unique<NodeSingleDeclareStatement>(node.tok);
        auto node_variable = std::make_unique<NodeVariable>(false, name_wo_ident+std::to_string(i)+".SIZE", VarType::Const, node.tok);
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
//    m_current_callback = m_init_callback;
    Token tok = Token(KEYWORD, "compiler_variable", 0, "");
    for(auto & var_name: m_compiler_variables) {
        auto node_variable = std::make_unique<NodeVariable>(false, var_name, VarType::Mutable, tok);
        node_variable->is_engine = true;
        node_variable->is_global = true;
        auto node_var_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_variable), nullptr, tok);
        node_var_declaration->to_be_declared->parent = node_var_declaration.get();
        node_var_declaration->accept(*this);
        m_local_declare_statements.push_back(statement_wrapper(std::move(node_var_declaration), m_init_callback->statements.get()));
    }
    for(auto &arr_name : m_return_arrays) {
        auto node_array = make_array(arr_name.second, m_current_callback_idx-1, tok, m_init_callback);
        node_array -> type = arr_name.first;
        node_array-> is_used = true;
        node_array->is_engine = true;
        node_array->is_global = true;
        auto node_arr_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), nullptr, tok);
        node_arr_declaration->to_be_declared->parent = node_arr_declaration.get();
        node_arr_declaration->accept(*this);
        m_local_declare_statements.push_back(statement_wrapper(std::move(node_arr_declaration), m_init_callback->statements.get()));
    }
    for(auto &arr_name : m_local_var_arrays) {
        auto node_array = make_array(arr_name.second, m_local_variables.size(), tok, m_init_callback);
        node_array -> type = arr_name.first;
        node_array-> is_used = true;
        node_array->is_engine = true;
        node_array->is_global = true;
        auto node_arr_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), nullptr, tok);
        node_arr_declaration->to_be_declared->parent = node_arr_declaration.get();
        node_arr_declaration->accept(*this);
        m_local_declare_statements.push_back(statement_wrapper(std::move(node_arr_declaration), m_init_callback->statements.get()));
    }
}

void ASTDesugar::declare_dummy_return_variable() {
    m_current_callback = m_init_callback;
    Token tok = Token(KEYWORD, "compiler_variable", 0, "");
    std::string dummy_name = "_return_dummy";
    auto node_return_dummy = std::make_unique<NodeVariable>(false, dummy_name, VarType::Mutable, tok);
    node_return_dummy->type = Unknown;
    node_return_dummy->is_engine = true;
    auto node_var_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_return_dummy), nullptr, tok);
    node_var_declaration->to_be_declared->parent = node_var_declaration.get();
    m_return_dummy_declaration = node_var_declaration->to_be_declared.get();
    m_local_declare_statements.push_back(statement_wrapper(std::move(node_var_declaration), m_init_callback));

    std::string local_var_dummy_name = "_local_dummy_";
    auto node_local_var_dummy = std::make_unique<NodeVariable>(false, local_var_dummy_name, VarType::Mutable, tok);
    node_local_var_dummy->type = Unknown;
    node_local_var_dummy->is_engine = true;
    auto node_local_var_declaration = std::make_unique<NodeSingleDeclareStatement>(std::move(node_local_var_dummy), nullptr, tok);
    node_local_var_declaration->to_be_declared->parent = node_local_var_declaration.get();
    m_local_var_dummy_declaration = node_local_var_declaration->to_be_declared.get();
    m_local_declare_statements.push_back(statement_wrapper(std::move(node_local_var_declaration), m_init_callback));


}

NodeFunctionDefinition* ASTDesugar::get_function_definition(NodeFunctionHeader *function_header) {
    auto it = m_function_lookup.find({function_header->name, (int)function_header->args->params.size()});
    if(it != m_function_lookup.end()) {
        it->second->is_used = true;
        return it->second;
    }
//    for(auto & function_def : m_function_definitions) {
//        if(function_def->header->name == function_header->name) {
//            if(function_def->header->args->params.size() == function_header->args->params.size()) {
//                function_def->is_used = true;
//                return function_def.get();
//            }
//        }
//    }
    return nullptr;
}

NodeFunctionHeader* ASTDesugar::get_builtin_function(NodeFunctionHeader *function) {
//    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
//                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
//                               return (func->name == function->name and
//                                       func->arg_ast_types.size() == function->args->params.size());
//                           });
//    if(it != m_builtin_functions.end()) {
//        return m_builtin_functions[std::distance(m_builtin_functions.begin(), it)].get();
//    }
    auto it = m_builtin_functions.find({function->name, (int)function->args->params.size()});
    if(it != m_builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* ASTDesugar::get_builtin_function(const std::string &function, int params) {
//    auto it = std::find_if(m_builtin_functions.begin(), m_builtin_functions.end(),
//                           [&](const std::unique_ptr<NodeFunctionHeader> &func) {
//                               return (func->name == function);
//                           });
//    if(it != m_builtin_functions.end()) {
//        return m_builtin_functions[std::distance(m_builtin_functions.begin(), it)].get();
//    }
    auto it = m_builtin_functions.find({function, params});
    if(it != m_builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* ASTDesugar::get_property_function(NodeFunctionHeader *function) {
    auto it = m_property_functions.find(function->name);
    if(it != m_property_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeUIControl* ASTDesugar::get_builtin_widget(const std::string &ui_control) {
	auto it = m_builtin_widgets.find(ui_control);
	if(it != m_builtin_widgets.end()) {
        return it->second.get();
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
    auto it = m_builtin_variables.find(to_upper(control_par));
    if(it == m_builtin_variables.end()) it = m_builtin_variables.find(to_upper("control_par_"+control_par));

    if(it != m_builtin_variables.end()) {
        return std::unique_ptr<NodeVariable>(static_cast<NodeVariable*>(it->second->clone().release()));
    }
    return nullptr;
}

std::unique_ptr<NodeAST> ASTDesugar::get_local_variable_substitute(const std::string& name) {
    for (auto rit = m_variable_scope_stack.rbegin(); rit != m_variable_scope_stack.rend(); ++rit) {
        auto it = rit->find(name);
        if(it != rit->end()) {
            return it->second->clone();
        }
    }
    return nullptr;
}

std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> ASTDesugar::get_function_inlines() {
    return std::move(m_function_inlines);
}

