//
// Created by Mathias Vatter on 07.06.24.
//

#pragma once

#include "ASTLowering.h"

///
class LoweringFunctionCall final : public ASTLowering {
private:
public:
	explicit LoweringFunctionCall(NodeProgram *program) : ASTLowering(program) {}

    /// Determining if function is property function -> inline property function
	/// Determining if function parameter needs to be wrapped in get_ui_id because of ui control
	/// Determining if function call is method constructor -> rename
	/// Lowering and re-connecting the builtin bool typecast to CKSP::__bool__()
	NodeAST * visit(NodeFunctionCall &node) override {
		node.bind_definition(m_program);

        if(node.kind == NodeFunctionCall::Kind::Property) {
            auto node_body = inline_property_function(node.get_definition()->header, std::move(node.function));
            node_body->accept(*this);
			node.get_definition()->call_sites.erase(&node);
            return node.replace_with(std::move(node_body));
        }
        if(node.kind == NodeFunctionCall::Kind::Builtin) {
            // get_ui_id lowering
            node.function->accept(*this);

			if (node.function->get_num_args() == 1) {
				if (node.function->name == "bool") {
					auto& arg = node.function->get_arg(0);
					if (arg->ty == TypeRegistry::Real) {
						auto int_call = DefinitionProvider::create_builtin_call("int", std::move(arg));
						int_call->bind_definition(m_program);
						node.function->set_arg(0, std::move(int_call));
					}
					node.function->name = "CKSP::__bool__";
					node.kind = NodeFunctionCall::Kind::UserDefined;
					node.collect_references();
					node.definition = m_program->def_provider->get_boolean_function(node.function->name, 1);
				}
			}

        	// check if we are in a user function, then replace exit function call with return stmt
        	if (node.function->get_num_args() == 0 and node.function->name == "exit") {
        		if (!m_program->function_call_stack.empty()) {
        			const auto& current_func = m_program->function_call_stack.top();
        			if (auto func = current_func.lock()) {
						func->num_return_stmts++;
        				auto block = std::make_unique<NodeBlock>(node.tok);
						block->add_as_stmt(std::make_unique<NodeReturn>(node.tok));
        				const auto return_stmt = block->get_last_statement()->cast<NodeReturn>();
        				return_stmt->definition = func;
        				func->return_stmts.push_back(return_stmt);
        				return node.replace_with(std::move(block));
        			}
        		}
        	}

        	return &node;
        }


		return &node;
	}

    /// lowering of set control statements from property functions
	NodeAST * visit(NodeSetControl &node) override {
		node.ui_id->accept(*this);
		node.value->accept(*this);
		return node.lower(m_program)->accept(*this);
	}

	NodeAST * visit(NodeGetControl &node) override {
		node.ui_id->accept(*this);
		return node.lower(m_program)->accept(*this);
	}


    NodeAST * visit(NodeVariableRef &node) override {
		if(needs_get_ui_id(node)) {
			return node.replace_with(std::move(wrap_in_get_ui_id(node)));
		}
		return &node;
    }

    NodeAST * visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		if(needs_get_ui_id(node)) {
			return node.replace_with(std::move(wrap_in_get_ui_id(node)));
		}
		return &node;
    }

	NodeAST * visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(needs_get_ui_id(node)) {
			return node.replace_with(std::move(wrap_in_get_ui_id(node)));
		}
		return &node;
	}


	static std::unique_ptr<NodeBlock> inline_property_function(const std::shared_ptr<NodeFunctionHeader>& property_function, std::unique_ptr<NodeFunctionHeaderRef> header_ref) {
        auto node_body = std::make_unique<NodeBlock>(header_ref->tok);
        for(int i = 1; i<header_ref->args->size(); i++) {
            auto node_set_control = std::make_unique<NodeSetControl>(
				header_ref->get_arg(0)->clone(),
				property_function->get_param(i)->name,
				std::move(header_ref->get_arg(i)),
				header_ref->tok
            );
            node_body->add_as_stmt(std::move(node_set_control));
        }
        return std::move(node_body);
    }

	static std::unique_ptr<NodeFunctionCall> wrap_in_get_ui_id(NodeReference& ref) {
		auto get_ui_id = DefinitionProvider::get_ui_id(clone_as<NodeReference>(&ref));
		get_ui_id->collect_references();
		// get_ui_id->bind_definition()
		return get_ui_id;
	}

	// static bool is_reference_to_function_param(const NodeReference& ref) {
	// 	if (const auto decl = ref.get_declaration()) {
	// 		return decl->is_function_param();
	// 	}
	// 	return false;
	// }

	// /// applies when reference declaration is a function formal parameter
	// static NodeAST* replace_get_ui_id(NodeFunctionCall* call) {
	// 	if (!call->is_builtin_kind()) return call;
	// 	// any get_ui_id call with an argument that is no ui_control an no reference to a function parameter can be removed
	// 	if (call->function->name != "get_ui_id") {
	// 		return call;
	// 	}
	//
	// 	if (auto ref = call->function->get_arg(0)->is_reference()) {
	// 		if (is_reference_to_function_param(*ref)) {
	// 			call->remove_references();
	// 			auto new_node = call->replace_with(std::move(call->function->get_arg(0)));
	// 			new_node->collect_references();
	// 			return new_node;
	// 		}
	// 	}
	// 	return call;
	// }

	/// returns true if reference is a ui_control, is a function argument and is the first argument
	/// of a builtin "control_par" function call, a property function call or any argument of a user-defined function call
	static bool needs_get_ui_id(const NodeReference& ref) {
		if (!ref.is_func_arg()) {
			return false;
		}

		if (auto decl = ref.get_declaration()) {
			if (auto param = decl->is_function_param()) {
				if (!param->is_pass_by_ref) {
					return false;
				}
				/// i would think if it is a function parameter, this was already handled by UIControlParamHandling
				return false;
			// if we are in a function, decl might not be markes as an ui_control data_type
			} else if (ref.data_type != DataType::UIControl) {
				return false;
			} else if (ref.data_type == DataType::UIArray) {
				// if it is a ui_array, we do not need to wrap it in get_ui_id
				return false;
			}
		}

		// if ref is a ui_control but also func arg and its func param is already passed-by-ref ->
		// we can not wrap it in get_ui_id, because it is already passed by reference


		// In NodeFunctionCall casten und weitere Bedingungen prüfen.
		const auto node = ref.parent->parent->parent;
		if (const auto func_call = node->cast<NodeFunctionCall>()) {
			if (func_call->kind == NodeFunctionCall::Kind::Builtin) {
				if (StringUtils::contains(func_call->function->name, "control_par")) {
					// Prüfen, ob diese Referenz das erste Argument ist.
					return func_call->function->get_arg(0).get() == &ref;
				}
			} else if (func_call->kind == NodeFunctionCall::Kind::Property) {
				// check if first arg
				return func_call->function->get_arg(0).get() == &ref;
			// } else if (func_call->kind == NodeFunctionCall::Kind::UserDefined) {
			// 	return true;
			}
		}
		return false;
	}



};