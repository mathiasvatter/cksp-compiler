//
// Created by Mathias Vatter on 15.05.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTGlobalScope : public ASTVisitor {
public:
	explicit ASTGlobalScope(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}

	void inline visit(NodeProgram& node) override {
		m_program = &node;
		for(auto & def : node.function_definitions) {
			m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

	}

	void inline visit(NodeFunctionCall& node) override {
		if(!node.definition) {
			if (auto function_def = get_function_definition(node.function.get())) {
				node.definition = function_def;
				m_current_function = function_def;
			} else if (auto builtin_func = m_def_provider->get_builtin_function(node.function.get())) {
				node.function->type = builtin_func->type;
				node.function->has_forced_parenth = builtin_func->has_forced_parenth;
				node.function->arg_ast_types = builtin_func->arg_ast_types;
				node.function->arg_var_types = builtin_func->arg_var_types;
				node.function->is_builtin = builtin_func->is_builtin;

				if(m_restricted_builtin_functions.find(builtin_func->name) != m_restricted_builtin_functions.end()) {
					if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
						CompileError(ErrorType::SyntaxError,"<"+builtin_func->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.", node.tok.line, "", "<"+m_current_callback->begin_callback+">", node.tok.file).print();
					} else {
						if(m_current_function)
							if(m_current_function->call.find(&node) != m_current_function->call.end())
								CompileError(ErrorType::SyntaxError,"<"+builtin_func->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks. Not in a called function.", node.tok.line, "", "<"+m_current_function->header->name+"> in <"+m_current_callback->begin_callback+">", node.tok.file).print();
					}
				}
			} else {
				CompileError(ErrorType::SyntaxError,"Function has not been declared.", node.tok.line, "", node.function->name, node.tok.file).exit();
			}
		}
		node.definition->accept(*this);
	}

private:
	DefinitionProvider* m_def_provider = nullptr;
	NodeProgram* m_program = nullptr;
	NodeFunctionDefinition* m_current_function = nullptr;

	std::stack<std::unique_ptr<NodeDataStructure>> m_passive_vars;

	std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> m_function_lookup;


	inline NodeFunctionDefinition* get_function_definition(NodeFunctionHeader *function_header) {
		auto it = m_function_lookup.find({function_header->name, (int)function_header->args->params.size()});
		if(it != m_function_lookup.end()) {
			it->second->is_used = true;
			return it->second;
		}
		return nullptr;
	}
};
