//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include <optional>

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
	/// stack of m_pointer_scope_stack depths at function entry; scopes above the top
	/// boundary belong to the function currently being traversed
	std::vector<size_t> m_function_scope_boundaries;

	std::unordered_map<StringTypeKey, std::unique_ptr<NodeFunctionCall>, StringTypeKeyHash> remove_scope() {
		if (!m_pointer_scope_stack.empty()) {
			auto scope = std::move(m_pointer_scope_stack.back());
			m_pointer_scope_stack.pop_back();
			return std::move(scope);
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
		// this pass runs after the return lowering: a lowered return (RETURN_FLAG/continue
		// or exit) jumps over the end-of-scope decrements. If the block leaves the function,
		// place the pending decrements before the exit sequence instead of appending them
		// behind it as dead code
		const bool exits_function = insert_pending_decrs_before_exit(node);
		// add delete statements for all local pointers
		if(node.scope) {
			auto temp_deletes = std::move(remove_scope());
			if (!exits_function) {
				for(auto & [key, del] : temp_deletes) {
					node.add_as_stmt(std::move(del));
				}
			}
		}
		node.flatten();
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.header->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		// scopes above this boundary belong to the function: a function exit must
		// decrement exactly the temporaries of these scopes, not the ones of the caller
		m_function_scope_boundaries.push_back(m_pointer_scope_stack.size());
		node.body->accept(*this);
		m_function_scope_boundaries.pop_back();
		return &node;
	}

private:
	struct FunctionExitSite {
		NodeBlock* block;
		size_t insert_index;
	};

	/// finds the function exit sequence the return lowering placed at the end of the block
	/// (following trailing sub-blocks): a return statement, an exit call, or a continue
	/// call preceded by its ReturnVar-marked RETURN_FLAG assignment. A bare continue is
	/// a loop continue or a RETURN_FLAG propagation check and no function exit
	static std::optional<FunctionExitSite> find_function_exit(NodeBlock& block) {
		NodeBlock* current = &block;
		while (current) {
			auto& stmts = current->statements;
			if (stmts.empty()) return std::nullopt;
			const auto last = stmts.back()->statement.get();
			if (last->get_node_type() == NodeType::Return) {
				return FunctionExitSite{current, stmts.size() - 1};
			}
			if (const auto call = cast_node<NodeFunctionCall>(last)) {
				if (call->function->name == "exit") {
					return FunctionExitSite{current, stmts.size() - 1};
				}
				if (call->function->name == "continue" and stmts.size() >= 2) {
					if (const auto assign = cast_node<NodeSingleAssignment>(stmts[stmts.size() - 2]->statement.get())) {
						if (assign->kind == NodeInstruction::ReturnVar) {
							return FunctionExitSite{current, stmts.size() - 2};
						}
					}
				}
				return std::nullopt;
			}
			current = cast_node<NodeBlock>(last);
		}
		return std::nullopt;
	}

	/// places clones of all pending temporary decrements of the current function before
	/// the function exit sequence at the end of the block, if there is one. Returns true
	/// when the block leaves the function
	bool insert_pending_decrs_before_exit(NodeBlock& node) {
		if (m_function_scope_boundaries.empty()) return false;
		const auto site = find_function_exit(node);
		if (!site) return false;

		size_t offset = 0;
		for (size_t depth = m_function_scope_boundaries.back(); depth < m_pointer_scope_stack.size(); ++depth) {
			for (auto& [key, del] : m_pointer_scope_stack[depth]) {
				auto stmt = std::make_unique<NodeStatement>(clone_as<NodeFunctionCall>(del.get()), del->tok);
				stmt->parent = site->block;
				stmt->statement->parent = stmt.get();
				site->block->statements.insert(
					site->block->statements.begin() + site->insert_index + offset, std::move(stmt));
				offset++;
			}
		}
		return true;
	}

public:

	// if constructor, get struct and increase constructor count
	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		auto decl = node.get_definition();
		if(decl and node.kind != NodeFunctionCall::Kind::Builtin) {
			if(!decl->visited) {
				FunctionCallStackScope diagnostic_frame(*m_program, node);
				decl->accept(*this);
			}
			decl->visited = true;
		}

		if(node.is_temporary_constructor) {
			auto &temp_ptr = node.function->get_arg(0);
			if(auto ref = temp_ptr->cast<NodeVariableRef>()) {
				auto decr_func = get_decr_func_call(node.function->name, temp_ptr->clone(), std::make_unique<NodeInt>(1, node.tok));
				decr_func->bind_definition(m_program);
				auto decl = decr_func->get_definition();
				if (!decl) {
					DefinitionProvider::internal_missing_definition_error(*decr_func);
				}
				// ref count methods need to be type lowered here
				// -> do not have a handle to them in ASTLowerTypes at this point and with temporary pointers
				// a situation can arise where decrease methods do not get visited by type lowering
				decl->do_type_lowering(m_program);
				// -> this pass will not go through the same function twice because of visited flag set previously
				m_pointer_scope_stack.back().try_emplace({ref->name, ref->ty}, std::move(decr_func));
			} else {
				auto error = Diagnostic(ErrorType::InternalError, "", "", node.tok);
				error.message = "Temporary constructor must have a variable reference as first argument.";
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
		std::string func_name = StringUtils::remove(object, OBJ_DELIMITER+"__init__")+OBJ_DELIMITER+"__decr__";
		auto call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(var->tok, std::move(var), std::move(num)),
				Token()
			),
			Token()
		);
		call->strategy = NodeFunctionCall::Strategy::ParameterStack;
		return call;
	}

};