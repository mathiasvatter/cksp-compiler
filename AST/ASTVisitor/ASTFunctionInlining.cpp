//
// Created by Mathias Vatter on 27.10.23.
//

#include "ASTFunctionInlining.h"

ASTFunctionInlining::ASTFunctionInlining(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {
}

void ASTFunctionInlining::visit(NodeProgram& node) {
    m_program = &node;
	node.update_function_lookup();
    for(auto & callback : node.callbacks) {
        callback->accept(*this);
    }
	evaluating_functions = true;
    for(auto & function : node.function_definitions) {
        if(function->is_used and function->header->args->params.empty() and !function->return_variable.has_value()) {
            m_functions_in_use.insert({function->header->name, function.get()});
            function->body->accept(*this);
            m_functions_in_use.erase(function->header->name);
        }
    }
	evaluating_functions = false;
    m_program->init_callback->statements->prepend_body(declare_dummy_variables());
	m_program->function_definitions = std::move(m_function_definitions);

    static FunctionInliningHelper function_inlining(std::move(m_function_inlines));
    node.accept(function_inlining);
}

void ASTFunctionInlining::visit(NodeCallback& node) {
    // empty the local var stack after init, because the idx can be reused now
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
//		function_def->call_sites.insert(&node);
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
//		node_function_def->body->update_token_data(node.tok);
		node_function_def->body->accept(*this);
		if (!node.function->args->params.empty()) m_substitution_stack.pop();
		m_functions_in_use.erase(node.function->name);
		m_function_call_stack.pop();
		function_def->call_sites.erase(&node);
		// has return variable
		if(node_function_def->return_variable.has_value()) {
			if(node.parent->parent->get_node_type() == NodeType::Block) {
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

bool ASTFunctionInlining::in_function() {
    return !m_function_call_stack.empty() || evaluating_functions;
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


void ASTFunctionInlining::visit(NodeWhile& node) {
    m_current_function_inline_statement = node.parent;
    node.condition->accept(*this);
    node.body->accept(*this);
}

void ASTFunctionInlining::visit(NodeIf& node) {
    m_current_function_inline_statement = node.parent;
    node.condition->accept(*this);
    node.if_body->accept(*this);
    node.else_body->accept(*this);
}

void ASTFunctionInlining::visit(NodeSelect& node) {
	m_current_function_inline_statement = node.parent;
	node.expression->accept(*this);
	for(const auto &cas: node.cases) {
		for(auto &c: cas.first) {
			c->accept(*this);
		}
		cas.second->accept(*this);
	}
}

void ASTFunctionInlining::visit(NodeBlock& node) {
    for(auto & stmt : node.statements) {
        stmt->accept(*this);
    }
    if(!node.scope) node.flatten();
}

void ASTFunctionInlining::visit(NodeUIControl &node) {
	node.control_var->accept(*this);
	node.params->accept(*this);
}

std::unique_ptr<NodeBlock> ASTFunctionInlining::declare_dummy_variables() {
	auto node_body = std::make_unique<NodeBlock>(Token());
    Token tok = Token(token::KEYWORD, "compiler_variable", -1, 0,"");
    std::string dummy_name = "_return_dummy";
    auto node_return_dummy = std::make_unique<NodeVariable>(std::optional<Token>(), dummy_name, TypeRegistry::Integer, DataType::Mutable, tok);
    node_return_dummy->ty = TypeRegistry::Unknown;
    node_return_dummy->is_engine = true;
    auto node_var_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node_return_dummy), nullptr, tok);
    node_var_declaration->variable->parent = node_var_declaration.get();
    m_return_dummy_declaration = static_cast<NodeDataStructure*>(node_var_declaration->variable.get());
    node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_var_declaration), tok));
	return node_body;
}

std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> ASTFunctionInlining::get_function_inlines() {
    return std::move(m_function_inlines);
}

