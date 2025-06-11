//
// Created by Mathias Vatter on 11.06.25.
//

#pragma once

#include "../ASTVisitor.h"

/* Determines if an actual parameter will be wrapped in a `get_ui_id` operation
 * This is the case if:
 * - the function body has set_control_par/get_control_par/property function calls wo the first arg wrapped in get_ui_id and
 *   the first argument is a function parameter -> go upwards to param and check if in all calls it is a ui control
 *
 */
class UIControlParamHandling final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

public:
	NodeAST* handle_ui_params(NodeFunctionDefinition& node) {
		return node.accept(*this);
	}

private:
	NodeAST* visit(NodeFunctionCall& node) override {
		if (is_get_ui_id_call(node)) {
			if (auto ref = node.function->get_arg(0)->is_reference()) {
				if (auto decl = ref->get_declaration()) {
					if (decl->is_function_param() and !decl->parent->cast<NodeFunctionParam>()->is_pass_by_ref) {
						auto error = CompileError(ErrorType::CompileWarning, "", "", node.tok);
						error.m_message = "Found <get_ui_id> call in function body with a parameter as argument. Due to pass-by-value"
								 " semantics this will not work as expected since <get_ui_id> can only be used with ui controls. Try passing"
						" ui controls by reference instead or using <get_ui_id> when passing the parameter to the function.";
						error.print();
					}
				}
			}
		}
		node.function->accept(*this);
		if (node.is_builtin_kind()) {
			if (node.kind == NodeFunctionCall::Kind::Property || StringUtils::contains(node.function->name, "control_par")) {
				auto& first_arg = node.function->get_arg(0);
				if (auto ref = first_arg->is_reference()) {
					std::vector<NodeReference*> references;
					find_declaration(*ref, references);
					wrap_in_get_ui_id(references);
				}
			}
		}
		return &node;
	}

	static bool is_in_get_ui_id(const NodeReference& ref) {
		if (auto header = ref.is_func_arg()) {
			if (auto func_call = header->parent->cast<NodeFunctionCall>()) {
				return is_get_ui_id_call(*func_call);
			}
		}
		return false;
	}

	static bool is_get_ui_id_call(NodeAST& node) {
		if (auto func_call = node.cast<NodeFunctionCall>()) {
			if (func_call->kind == NodeFunctionCall::Kind::Builtin && func_call->function->name == "get_ui_id") {
				return true;
			}
		}
		return false;
	}


	/// wrap in get_ui_id if all references are either ui controls or ui control arrays -> only wrap single ui_controls
	static void wrap_in_get_ui_id(const std::vector<NodeReference*>& references) {
		for (auto& ref : references) {
			// see if ref is not already wrapped in get_ui_id
			if (is_in_get_ui_id(*ref)) {
				continue;
			}
			if (auto decl = ref->get_declaration()) {
				if (decl->data_type == DataType::UIArray) {
					continue;
				} else if (decl->data_type == DataType::UIControl) {
					// wrap in get_ui_id
					auto get_ui_id = DefinitionProvider::get_ui_id(clone_as<NodeReference>(ref));
					get_ui_id->collect_references();
					ref->replace_with(std::move(get_ui_id));
				} else {
					// do not wrap in get_ui_id
					continue;
				}
			}
		}
	}


	void find_declaration(NodeReference& ref, std::vector<NodeReference*> &references) {
		if (auto decl = ref.get_declaration()) {
			if (decl->is_function_param()) {
				auto param = decl->parent->cast<NodeFunctionParam>();
				if (!param->is_pass_by_ref) {
					find_original_references(*param, references);
				}
			} else if (decl->parent and decl->parent->cast<NodeUIControl>()) {
				references.push_back(&ref);
			}
		}
	}

	// try to propagate further if actual param is either get_ui_id or direct reference to ui control
	void find_original_references(NodeFunctionParam& param, std::vector<NodeReference*> &references) {
		auto index = param.get_index();
		if (index < 0) return;
		auto func_def = param.parent->parent->cast<NodeFunctionDefinition>();
		if (!func_def) return;
		for (auto& call : func_def->call_sites) {
			auto& arg = call->function->get_arg(index);
			if ( auto ref = arg->is_reference()) {
				find_declaration(*ref, references);
			}
		}
	}


};

class FunctionHasControlFunctions final : public ASTVisitor {
	bool has_control_functions = false;



};