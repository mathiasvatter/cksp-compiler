//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTFunctionInlining.h"

ASTFunctionInlining::ASTFunctionInlining(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {
    m_local_declare_statements = std::make_unique<NodeBody>(Token());
//    m_compiler_variable_declare_statements = std::make_unique<NodeBody>(Token());
}

void ASTFunctionInlining::visit(NodeProgram& node) {
    m_program = &node;
	node.update_function_lookup();
    declare_dummy_variables();
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
    // declare after callback visiting because of size of m_local_variables
    declare_local_var_arrays();
	evaluating_functions = true;
    for(auto & function : node.function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            m_functions_in_use.insert({function->header->name, function.get()});
            function->body->accept(*this);
            m_functions_in_use.erase(function->header->name);
        }
    }
	evaluating_functions = false;

    // add local variables from function bodies to beginning of m_program->init_callback
//    m_compiler_variable_declare_statements->set_child_parents();
//    m_program->init_callback->statements->prepend_body(std::move(m_compiler_variable_declare_statements));
    m_local_declare_statements->set_child_parents();
    m_program->init_callback->statements->prepend_body(std::move(m_local_declare_statements));

	m_program->function_definitions = std::move(m_function_definitions);

    static FunctionInliningHelper function_inlining(std::move(m_function_inlines));
    node.accept(function_inlining);
}

void ASTFunctionInlining::visit(NodeCallback& node) {
    // empty the local var stack after init, because the idx can be reused now
    if(m_current_callback == m_program->init_callback) {
        m_local_variables = std::stack<std::string>();
    }
    m_current_callback = &node;
    if(node.callback_id)
        node.callback_id->accept(*this);
    node.statements->accept(*this);
    m_current_callback_idx++;

}
void ASTFunctionInlining::visit(NodeNDArray& node) {
	CompileError(ErrorType::SyntaxError, "Found <nd array>. This data structure should not exist anymore after lowering.", node.tok.line, "", "", node.tok.file).exit();
}

void ASTFunctionInlining::visit(NodeArrayRef& node) {
    if(node.index) node.index->accept(*this);

    // local variable substitution
//    if(!m_variable_scope_stack.empty()) {
//        if(auto substitute = get_local_variable_substitute(node.name)) {
//            node.name = substitute->name;
//            node.is_local = true;
//        }
//    }
    // function args substitution
    if(!m_substitution_stack.empty()) {
		if (auto substitute = get_substitute(node.name)) {
			if (auto substitute_ref = cast_node<NodeReference>(substitute.get())) {
				node.name = substitute_ref->name;
			} else {
				node.replace_with(std::move(substitute));
				return;
			}
		// for namespaces and methods
		} else {
            auto namespaces = get_namespaces(node.name);
			if(namespaces.size() == 1) return;
            std::string new_name;
            if (auto subst = get_substitute(namespaces.at(0))) {
                new_name += subst->get_string();
            } else {
                new_name += namespaces.at(0);
            }
            for(size_t i = 1; i<namespaces.size(); i++) {
                new_name += "."+namespaces.at(i);
            }
            node.name = new_name;
		}
    }

}

void ASTFunctionInlining::visit(NodeFunctionHeader& node) {
    node.args->accept(*this);
    // substitution
    if (!m_substitution_stack.empty()) {
        if (auto substitute = get_substitute(node.name)) {
            if(auto substitute_ref = cast_node<NodeReference>(substitute.get())) {
                node.name = substitute_ref->name;
                return;
            } else {
                auto error = CompileError(ErrorType::SyntaxError, "Cannot substitute Function name with <non-datastructure>", "", node.tok);
                error.m_expected = "<Reference>";
                error.m_got = substitute->get_string();
                error.exit();
            }
        }
    }
}

void ASTFunctionInlining::visit(NodeVariableRef& node) {
    // local variable substitution
    // do local variable substitution
//    if(!m_variable_scope_stack.empty()) {
//        if(auto substitute = get_local_variable_substitute(node.name)) {
//            if(substitute->type == ASTType::Unknown)
//                substitute->type = node.type;
//            node.replace_with(std::move(substitute));
//            return;
//        }
//    }

	// function args substitution
	if(!m_substitution_stack.empty()) {
		if (auto substitute = get_substitute(node.name)) {
            node.replace_with(std::move(substitute));
            return;
		// for namespaces and methods
		} else {
            auto namespaces = get_namespaces(node.name);
			if(namespaces.size() == 1) return;
            std::string new_name;
            if (auto subst = get_substitute(namespaces.at(0))) {
                new_name += subst->get_string();
            } else {
                new_name += namespaces.at(0);
            }
            for(size_t i = 1; i<namespaces.size(); i++) {
                new_name += "."+namespaces.at(i);
            }
            node.name = new_name;
		}
	}
}

void ASTFunctionInlining::visit(NodeFunctionCall& node) {
    if(node.is_call and !node.function->args->params.empty()) {
        CompileError(ErrorType::SyntaxError,
         "Found incorrect amount of function arguments when using <call>.", node.tok.line, "0", std::to_string(node.function->args->params.size()), node.tok.file).exit();
    }
    if(m_functions_in_use.find(node.function->name) != m_functions_in_use.end()) {
        // recursive function call detected
        CompileError(ErrorType::SyntaxError,"Recursive function call detected. Calling functions inside their definition is not allowed.", node.tok.line, "", node.function->name, node.tok.file).exit();
    }
    NodeAST* old_function_inline_statement = m_current_function_inline_statement;
    auto nearest_statement = static_cast<NodeStatement*>(m_current_function_inline_statement);
    node.function->accept(*this);

    // substitution start
    node.get_definition(m_program);
    if(node.kind == NodeFunctionCall::Kind::Property) {
        CompileError(ErrorType::InternalError,"Found undefined property function.", "", node.tok).exit();
    }
    if(!node.definition or node.kind == NodeFunctionCall::Kind::Undefined) {
        CompileError(ErrorType::InternalError,"Missing definition pointer in <Function Call>.", "", node.tok).exit();
    }

	if(node.kind == NodeFunctionCall::Kind::Builtin) {
		if(m_restricted_builtin_functions.find(node.function->name) != m_restricted_builtin_functions.end()) {
			if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
				CompileError(ErrorType::SyntaxError,"<"+node.function->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.", node.tok.line, "", "<"+m_current_callback->begin_callback+">", node.tok.file).print();
			} else {
				if(m_current_function)
					if(m_current_function->call_sites.find(&node) != m_current_function->call_sites.end())
						CompileError(ErrorType::SyntaxError,"<"+node.function->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks. Not in a called function.", node.tok.line, "", "<"+m_current_function->header->name+"> in <"+m_current_callback->begin_callback+">", node.tok.file).print();
			}
		}
	}

	if(node.kind == NodeFunctionCall::Kind::UserDefined) {
		auto function_def = node.definition;
		if(node.is_call and function_def->return_variable.has_value()) {
			CompileError(ErrorType::SyntaxError, "Found incorrect use of return variable when using <call>.", node.tok.line, "", "", node.tok.file).exit();
		}
		function_def->call_sites.insert(&node);
		m_current_function = function_def;
		// inline if current call is <on init>
		if(m_current_callback != m_program->init_callback) {
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

		auto node_function_def = clone_as<NodeFunctionDefinition>(function_def);
		m_function_call_stack.push(node.function->name);
		m_functions_in_use.insert({node.function->name, function_def});
		node_function_def->parent = node.parent;
		if (!node.function->args->params.empty()) {
			auto substitution_map = get_substitution_map(node_function_def->header.get(), node.function.get());
			m_substitution_stack.push(std::move(substitution_map));
		}
		node_function_def->body->update_token_data(node.tok);
		node_function_def->body->accept(*this);
		if (!node.function->args->params.empty()) m_substitution_stack.pop();
		m_functions_in_use.erase(node.function->name);
		m_function_call_stack.pop();
		function_def->call_sites.erase(&node);
		// has return variable
		if(node_function_def->return_variable.has_value()) {
			if(node.parent->parent->get_node_type() == NodeType::Body) {
				CompileError(ErrorType::SyntaxError,"Function returns a value. Move function into assign statement.", node.tok.line, node_function_def->return_variable.value()->get_string(), "<assignment>", node.tok.file).exit();
			}
			if (!node_function_def->body->statements.empty()) {
				auto node_assignment = cast_node<NodeSingleAssignment>(node_function_def->body->statements[0]->statement.get());
				// strict inlining method only if function body is assign statement
				if (node_function_def->body->statements.size() == 1 and node_assignment) {
					if (node_assignment->l_value->get_string() == node_function_def->return_variable.value()->get_string()) {
						node.replace_with(std::move(node_assignment->r_value));
						return;
					} else {
						CompileError(ErrorType::SyntaxError,
                                     "Given return variable of function could not be found in function body.",
                                     node.tok.line, node_function_def->return_variable.value()->get_string(),
                                     node_assignment->l_value->get_string(), node.tok.file).exit();
					}
					// new inlining method
				} else {
					std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_map;
					auto node_return_var_name =
						"_return_var_" + std::to_string(nearest_statement->function_inlines.size());
					auto node_return_var = std::make_unique<NodeVariableRef>(node_return_var_name, node.tok);
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
					auto node_statement = std::make_unique<NodeStatement>(std::move(node_function_def->body), node.tok);
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
				node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
			}
		} else {
			m_function_definitions.push_back(std::move(node_function_def));
		}
	}
}


std::unordered_map<std::string, std::unique_ptr<NodeAST>> ASTFunctionInlining::get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call) {
    std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_vector;
    for(int i= 0; i<definition->args->params.size(); i++) {
        auto &var = definition->args->params[i];
        std::pair<std::string, std::unique_ptr<NodeAST>> pair;
        if(auto node_data_structure = cast_node<NodeDataStructure>(var.get())) {
            pair.first = node_data_structure->name;
        } else {
            CompileError(ErrorType::SyntaxError,
             "Found incorrect parameter definitions in <function header>. Unable to substitute function arguments. Only <data structures> can be substituted.", definition->tok.line, "<keyword>", var->tok.val,definition->tok.file).print();
            exit(EXIT_FAILURE);
        }
        pair.second = std::move(call->args->params[i]);
        substitution_vector.insert(std::move(pair));
    }
    return substitution_vector;
}

std::unique_ptr<NodeAST> ASTFunctionInlining::get_substitute(const std::string& name) {
    const auto & map = m_substitution_stack.top();
    auto it = map.find(name);
    if(it != map.end()) {
        auto substitute = it->second->clone();
        substitute->update_parents(nullptr);
        return substitute;
    }
    return nullptr;
}

void ASTFunctionInlining::visit(NodeSingleDeclaration& node) {
	m_current_function_inline_statement = node.parent;
	node.variable ->accept(*this);
	if(node.value)
		node.value -> accept(*this);
}

//void ASTFunctionInlining::visit(NodeSingleDeclaration& node) {
//    m_current_function_inline_statement = node.parent;
//
//    auto node_data_struct = node.variable.get();
//
//	if(in_function()) node_data_struct->is_local = true;
//	if(m_current_callback != m_program->init_callback) node_data_struct->is_local = true;
//	auto persistence = node_data_struct->persistence;
//	auto is_local = node_data_struct->is_local;
//	auto is_global = node_data_struct->is_global;
//	if(node_data_struct->is_global) node_data_struct->is_local = false;
//	auto is_const = node_data_struct->data_type == DataType::Const;
//	auto is_engine = node_data_struct->is_engine;
//
//    if(is_const and !node.r_value)
//        CompileError(ErrorType::SyntaxError,
//                     "Found <const> declaration without value assignment.", node.tok.line, "<assignment>", "",node.tok.file).exit();
//	// if currently in function inlining
//	if(is_local and !is_global and !is_const and !is_engine) {
//
//		node.variable ->accept(*this);
//        auto new_name = "_"+node.variable->get_string()+"_"+std::to_string(m_local_variables.size());
//		std::unique_ptr<NodeReference> node_substitute;
//		if(node.variable->get_node_type() == NodeType::Array) {
//			auto node_local_array = node.variable->to_reference();
//			node_local_array->name = new_name;
//			node_substitute = std::move(node_local_array);
//		} else if(node.variable->get_node_type() == NodeType::Variable) {
//			auto node_local_var = node.variable->to_reference();
//			node_local_var->name = "_local_dummy_"+std::to_string(m_local_variables.size());
//            node_local_var->declaration = m_local_var_dummy_declaration;
//			node_substitute = std::move(node_local_var);
//		}
//        m_local_variables.push(new_name);
//        if(m_variable_scope_stack.back().find(node.variable->get_string()) != m_variable_scope_stack.back().end()) {
//            CompileError(ErrorType::SyntaxError,"Local Variable was already declared in this scope", node.tok.line, "", node.variable->get_string(),node.tok.file).exit();
//        }
//		m_variable_scope_stack.back().insert({node.variable->get_string(), std::move(node_substitute)});
//	}
//    auto node_variable = cast_node<NodeVariable>(node.variable.get());
//    auto node_array = cast_node<NodeArray>(node.variable.get());
//
//	node.variable ->accept(*this);
//
//    // in case node.r_value is function substitution -> then this node gets replaced
//    if(&node == m_current_node_replaced) {
//        m_current_node_replaced = nullptr;
//        return;
//    }
//
//    if(node.r_value)
//        node.r_value -> accept(*this);
//
//    // add make_persistent and read_persistent_var
//    auto node_body = std::make_unique<NodeBody>(node.tok);
//
//    // see if r_value is param_list
//    NodeParamList* param_list = nullptr;
//    if(node.r_value and node_array) param_list = cast_node<NodeParamList>(node.r_value.get());
//    // make param list if it is array declaration and no param list as r_value
//    if(node_array and node.r_value and !param_list) {
//        auto node_param_list = std::make_unique<NodeParamList>(node.r_value->tok);
//        node_param_list->params.push_back(std::move(node.r_value));
//        node_param_list->parent = &node;
//        node.r_value = std::move(node_param_list);
//    }
//
//    if(node_variable) {
//        // a list of values is assigned to a declared variable
//        if(param_list and param_list->params.size() > 1) {
//            CompileError(ErrorType::SyntaxError,"Unable to assign a list of values to variable.", node.tok.line, "single value", "list of values",node.tok.file).exit();
//        }
//    }
//
//    if(persistence) {
//		NodeDataStructure* to_be_declared_ptr = node.variable.get();
//		// clear index if it is array because of: make_persistent_var(arr[124]) <-
//		std::unique_ptr<NodeArray> node_array_copy;
//		if(node_array) {
//			node_array_copy = clone_as<NodeArray>(node.variable.get());
//			to_be_declared_ptr = node_array_copy.get();
//		}
//        add_vector_to_statement_list(node_body, add_read_functions(persistence.value(), to_be_declared_ptr, node_body.get()));
//    }
//
//    // special treatment if declaration is inside function -> move to init_callback
//    if(is_local and !is_engine) {
//        std::unique_ptr<NodeAST> stmt;
//        std::unique_ptr<NodeAST> r_value = nullptr;
//        if (node.r_value and !is_const and !node_array) {
////			auto cloned_var = clone_as<NodeVariable>(node.variable.get());
////			cloned_var->is_reference = true;
//            auto node_assignment = std::make_unique<NodeSingleAssignment>(node.variable->to_reference(),
//                                                                               std::move(node.r_value), node.tok);
//            node_assignment->accept(*this);
//            stmt = std::move(node_assignment);
//        } else {
//            r_value = std::move(node.r_value);
//            auto node_dead_end = std::make_unique<NodeDeadCode>(node.tok);
//            stmt = std::move(node_dead_end);
//        }
//        if(node_array or is_global or is_const) {
//            std::string var_name_hash = node.variable->get_string();
//            // check if this var has already been moved to m_declare_statements_to_move
//            auto it = m_local_already_declared_vars.find(var_name_hash);
//            bool local_array_already_declared = it != m_local_already_declared_vars.end();
//            if (!local_array_already_declared) {
//                auto node_declaration = std::make_unique<NodeSingleDeclaration>(
//					std::move(node.variable),
//					std::move(r_value), node.tok
//				);
//                node_body->statements.insert(node_body->statements.begin(),
//                                                       statement_wrapper(std::move(node_declaration), m_program->init_callback));
//                auto statement = statement_wrapper(std::move(node_body), m_program->init_callback);
//                statement->update_parents(m_program->init_callback);
//                m_local_declare_statements->statements.push_back(std::move(statement));
//                m_local_already_declared_vars.insert(var_name_hash);
//            }
//        }
//        node.replace_with(std::move(stmt));
//    } else {
//        node_body->statements.insert(node_body->statements.begin(), statement_wrapper(node.clone(), node_body.get()));
//        node_body->update_parents(node.parent);
//        node.replace_with(std::move(node_body));
//    }
//
//}

bool ASTFunctionInlining::in_function() {
    return !m_function_call_stack.empty() || evaluating_functions;
}

std::vector<std::unique_ptr<NodeStatement>> ASTFunctionInlining::add_read_functions(const Token& persistence, NodeDataStructure* var, NodeAST* parent) {
    std::vector<std::unique_ptr<NodeStatement>> statements;

    for(auto &pers : m_persistences) {
        if(persistence.type == pers.first) {
            for(auto &pers_func : pers.second) {
                auto node_function = m_def_provider->get_builtin_function(pers_func, 1)->clone();
                auto make_persistent = std::unique_ptr<NodeFunctionHeader>(static_cast<NodeFunctionHeader*>(node_function.release()));
                make_persistent->args->params.clear();
				auto node_var = var->to_reference();
                make_persistent->args->params.push_back(std::move(node_var));
                statements.push_back(std::make_unique<NodeStatement>(std::move(make_persistent), var->tok));
            }
        }
    }
    return std::move(statements);
}

void ASTFunctionInlining::visit(NodeSingleAssignment& node) {
    m_current_function_inline_statement = node.parent;
    node.l_value ->accept(*this);

    // needed to add check, because if node.l_value is getControlStatement, this whole node will be replaced
    if(&node == m_current_node_replaced) {
        m_current_node_replaced = nullptr;
        return;
    }

    node.r_value -> accept(*this);
}

void ASTFunctionInlining::visit(NodeParamList& node) {
    for(auto & param : node.params) {
        param->accept(*this);
    }
}

void ASTFunctionInlining::visit(NodeGetControl& node) {
	CompileError(ErrorType::SyntaxError, "Found <get_control_par> statement. This statement should not exist anymore after lowering.", node.tok.line, "", "", node.tok.file).exit();
}


void ASTFunctionInlining::visit(NodeWhileStatement& node) {
    m_current_function_inline_statement = node.parent;
    node.condition->accept(*this);
    node.statements->accept(*this);
}

void ASTFunctionInlining::visit(NodeIfStatement& node) {
    m_current_function_inline_statement = node.parent;
    node.condition->accept(*this);
    node.statements->accept(*this);
    node.else_statements->accept(*this);
}

void ASTFunctionInlining::visit(NodeSelectStatement& node) {
	m_current_function_inline_statement = node.parent;
	node.expression->accept(*this);
	m_variable_scope_stack.emplace_back();
	for(const auto &cas: node.cases) {
		for(auto &c: cas.first) {
			c->accept(*this);
		}
		cas.second->accept(*this);
	}
	m_variable_scope_stack.pop_back();
}

void ASTFunctionInlining::visit(NodeBody& node) {
	if(node.scope) m_variable_scope_stack.emplace_back();
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
	if(node.scope) m_variable_scope_stack.pop_back();

    if(!node.scope) node.cleanup_body();
}

void ASTFunctionInlining::visit(NodeUIControl &node) {

	node.control_var->accept(*this);
	node.params->accept(*this);

//	// check if persistence
//	std::optional<Token> persistence;
//	auto node_array = cast_node<NodeArray>(node.control_var.get());
//	if(node_array) persistence = node_array->persistence;
//	auto node_variable = cast_node<NodeVariable>(node.control_var.get());
//	if(node_variable) persistence = node_variable->persistence;
//
//	// add make_persistent and read_persistent_var
//	auto node_body = std::make_unique<NodeBody>(node.tok);
//
//	if(persistence) {
//		add_vector_to_statement_list(node_body, add_read_functions(persistence.value(), node.control_var.get(), node_body.get()));
//	}
//	// split declare statement into declare + assign statement
//	auto node_declare_statement = std::unique_ptr<NodeSingleDeclaration>(static_cast<NodeSingleDeclaration*>(node.parent->clone().release()));
//	if(node_declare_statement->r_value) {
//		auto node_assign_statement = std::make_unique<NodeSingleAssignment>(std::move(node.control_var), std::move(node_declare_statement->r_value), node.tok);
//		node_body->statements.push_back(statement_wrapper(std::move(node_assign_statement), node_body.get()));
//	}
//	node_body->statements.insert(node_body->statements.begin(), statement_wrapper(std::move(node_declare_statement), node_body.get()));
//	node_body->update_parents(node.parent->parent);
//
//    m_current_node_replaced = node.parent;
//	node.parent->replace_with(std::move(node_body));
}

void ASTFunctionInlining::declare_local_var_arrays() {
    Token tok = Token(token::KEYWORD, "compiler_variable", 0, 0,"");
    for(auto &arr_name : m_local_var_arrays) {
		auto node_array = std::make_unique<NodeArray>(
			std::nullopt,
			arr_name.second,
            arr_name.first,
			DataType::Array,
			std::make_unique<NodeInt>(std::max(1,(int)m_local_variables.size()), tok),
			tok);

        node_array->is_used = true;
        node_array->is_engine = true;
        node_array->is_global = true;
        auto node_arr_declaration = std::make_unique<NodeSingleDeclaration>(
			std::move(node_array),
			nullptr, tok);
        node_arr_declaration->variable->parent = node_arr_declaration.get();
        node_arr_declaration->accept(*this);
        m_local_declare_statements->statements.push_back(std::make_unique<NodeStatement>(std::move(node_arr_declaration), tok));
    }
}

void ASTFunctionInlining::declare_dummy_variables() {
    m_current_callback = m_program->init_callback;
    Token tok = Token(token::KEYWORD, "compiler_variable", -1, 0,"");
    std::string dummy_name = "_return_dummy";
    auto node_return_dummy = std::make_unique<NodeVariable>(std::optional<Token>(), dummy_name, TypeRegistry::Integer, DataType::Mutable, tok);
    node_return_dummy->type = ASTType::Unknown;
    node_return_dummy->is_engine = true;
    auto node_var_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node_return_dummy), nullptr, tok);
    node_var_declaration->variable->parent = node_var_declaration.get();
    m_return_dummy_declaration = static_cast<NodeDataStructure*>(node_var_declaration->variable.get());
    m_local_declare_statements->statements.push_back(std::make_unique<NodeStatement>(std::move(node_var_declaration), tok));

//    std::string local_var_dummy_name = "_local_dummy_";
//    auto node_local_var_dummy = std::make_unique<NodeVariable>(std::optional<Token>(), local_var_dummy_name, DataType::Mutable, tok);
//    node_local_var_dummy->type = ASTType::Unknown;
//    node_local_var_dummy->is_engine = true;
//    auto node_local_var_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node_local_var_dummy), nullptr, tok);
//    node_local_var_declaration->variable->parent = node_local_var_declaration.get();
//    m_local_var_dummy_declaration = static_cast<NodeDataStructure*>(node_local_var_declaration->variable.get());
//    m_local_declare_statements->statements.push_back(std::make_unique<NodeStatement>(std::move(node_local_var_declaration), tok));

}

//NodeFunctionDefinition* ASTFunctionInlining::get_function_definition(NodeFunctionHeader *function_header) {
//    auto it = m_function_lookup.find({function_header->name, (int)function_header->args->params.size()});
//    if(it != m_function_lookup.end()) {
//        it->second->is_used = true;
//        return it->second;
//    }
//    return nullptr;
//}

std::unique_ptr<NodeReference> ASTFunctionInlining::get_local_variable_substitute(const std::string& name) {
    for (auto rit = m_variable_scope_stack.rbegin(); rit != m_variable_scope_stack.rend(); ++rit) {
        auto it = rit->find(name);
        if(it != rit->end()) {
            return clone_as<NodeReference>(it->second.get());
        }
    }
    return nullptr;
}

std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> ASTFunctionInlining::get_function_inlines() {
    return std::move(m_function_inlines);
}

