//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTVisitor.h"


/**
 * This class is implemented after the rewriting of return functions to standalone functions
 * It tackles previously marked constructors as temp_constructors (that only get used in function args)
 * Saves the engine temp return variabls of these constructors and adds delete statements at the end of scope.
 *
 */
class ASTTemporaryPointerScope : public ASTVisitor {
private:
	DefinitionProvider* m_def_provider = nullptr;
	std::vector<std::unordered_map<StringTypeKey, std::unique_ptr<NodeFunctionCall>, StringTypeKeyHash>> m_pointer_scope_stack;

	std::unordered_map<StringTypeKey, std::unique_ptr<NodeFunctionCall>, StringTypeKeyHash> remove_scope() {
		if (!m_pointer_scope_stack.empty()) {
			auto scope = std::move(m_pointer_scope_stack.back());
			m_pointer_scope_stack.pop_back();
			return scope;
		}
		return {}; // Leere Map, wenn kein Scope vorhanden ist
	}

public:
	explicit ASTTemporaryPointerScope(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	};

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;

		// most func defs will be visited when called, keeping local scopes in mind
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}

		node.reset_function_visited_flag();

		return &node;
	}

	NodeAST* visit(NodeBlock &node) override {
		if(node.scope) m_pointer_scope_stack.emplace_back();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		// add delete statements for all local pointers
		if(node.scope) {
			auto temp_deletes = remove_scope();
			for(auto & [key, del] : temp_deletes) {
				node.add_as_stmt(std::move(del));
			}
		}
		node.flatten();
		return &node;
	}

	// if constructor, get struct and increase constructor count
	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		if(node.get_definition() and node.kind != NodeFunctionCall::Kind::Builtin) {
			if(!node.get_definition()->visited) node.get_definition()->accept(*this);
			node.get_definition()->visited = true;
		}

		if(node.is_temporary_constructor) {
			auto &temp_ptr = node.function->get_arg(0);
			if(auto ref = temp_ptr->cast<NodeVariableRef>()) {
				auto decr_func = get_decr_func_call(node.function->name, temp_ptr->clone(), std::make_unique<NodeInt>(1, node.tok));
				m_pointer_scope_stack.back()[{ref->name, ref->ty}] =  std::move(decr_func);
			} else {
				auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
				error.m_message = "Temporary constructor must have a variable reference as first argument.";
				error.exit();
			}
		}


		return &node;
	};

private:
	/**
	 * creates call to any ref count function __decr__
	 */
	static std::unique_ptr<NodeFunctionCall> get_decr_func_call(const std::string& object, std::unique_ptr<NodeAST> var, std::unique_ptr<NodeAST> num) {
		std::string func_name = object+OBJ_DELIMITER+"__decr__";
		return std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(var->tok, std::move(var), std::move(num)),
				Token()
			),
			Token()
		);
	}

};