//
// Created by Mathias Vatter on 08.08.24.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Takes a function call and inlines the function body into the call after substituting all parameters
 */
class FunctionInlining final : public ASTVisitor {
	std::vector<NodeSingleAssignment*> m_immutable_param_stack_assignments; // assignments like 0 := param -> to be removed
public:
	explicit FunctionInlining(NodeProgram* main) {
		m_program = main;
	}

	NodeAST* do_function_inlining(NodeFunctionCall& call) {
		m_immutable_param_stack_assignments.clear();
		return call.accept(*this);
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.is_builtin_kind()) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Function of builtin kind can not be inlined.";
			return &node;
		}
		auto definition = node.get_definition();
		if(!definition) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "FunctionInlining : Function node has to have a definition.";
		}

		// deal here with expression functions
		if(node.strategy == NodeFunctionCall::Strategy::ExpressionFunc) {
			auto node_func_body = clone_as<NodeBlock>(definition->body.get());
			m_substitution_stack.push(get_substitution_map(*definition->header, *node.function));
			node_func_body->accept(*this);
			m_substitution_stack.pop();
			return node.replace_with(get_expression_return(node_func_body.get()));
		} else if (node.strategy == NodeFunctionCall::Strategy::Call) {
			return &node;
		}



		// check if FunctionCall is in statement and not assigned or in a condition etc (Handled bei ReturnFunctionRewriting class)
		if(!node.parent->cast<NodeStatement>()) {
			CompileError(ErrorType::InternalError, "FunctionInlining : Function node was not rewritten and is not within <Statement>.", "", node.tok).exit();
		}

		// inlining process
		// can not be set to false here, because some func calls might be "called" and some not
		auto node_func_body = clone_as<NodeBlock>(definition->body.get());
		m_substitution_stack.push(get_substitution_map(*definition->header, *node.function));
		node_func_body->accept(*this);
		m_substitution_stack.pop();

		for (auto & assignment : m_immutable_param_stack_assignments) {
			assignment->remove_node();
		}
		m_immutable_param_stack_assignments.clear();

		return node.replace_with(std::move(node_func_body));
	}

private:
	static std::unique_ptr<NodeAST> get_expression_return(const NodeBlock* body) {
		const auto stmt = body->statements[0]->statement.get();
		if(const auto ret = stmt->cast<NodeReturn>()) {
			return std::move(ret->return_variables[0]);
		}
		auto error = CompileError(ErrorType::InternalError, "", "", body->tok);
		error.m_message = "Function is not a return-only function";
		error.exit();
		return nullptr;
	}

	/// do substitution
	NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
		return do_substitution(&node);
	}
	/// do substitution
	NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		return do_substitution(&node);
	}
	/// do substitution
	NodeAST *visit(NodeVariableRef &node) override {
		return do_substitution(&node);
	}
	/// do substitution
	NodeAST *visit(NodeFunctionHeaderRef &node) override {
		if(node.args) node.args->accept(*this);
		return do_substitution(&node);
	}

	static std::unordered_map<std::string, std::unique_ptr<NodeAST>> get_substitution_map(const NodeFunctionHeader& definition, const NodeFunctionHeaderRef& call) {
		std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_map;
		substitution_map.reserve(definition.params.size());
		for (size_t i = 0; i < definition.params.size(); ++i) {
			const auto& var = definition.get_param(i);
			// Add raw pointer of data structure to map
			substitution_map[var->name] = std::move(call.get_arg(i));
		}
		return substitution_map;
	}


	// /// returns empty string if ndarray constant pattern is not found
	// static std::string get_ndarray_constant_base(const std::string& input) {
	// 	// Finde die Position des letzten Punktes
	// 	const size_t pos = input.find_last_of('.');
	// 	if (pos == std::string::npos) {
	// 		return ""; // Kein Punkt gefunden, also kein gültiges Muster
	// 	}
	//
	// 	// Überprüfe, ob der Teil nach dem Punkt dem erwarteten Muster entspricht
	// 	std::string suffix = input.substr(pos + 1);
	// 	if (suffix.size() > 6 && suffix.substr(0, 6) == "SIZE_D" && std::all_of(suffix.begin() + 6, suffix.end(), ::isdigit)) {
	// 		return input.substr(0, pos); // Der Teil vor dem Punkt wird zurückgegeben
	// 	}
	//
	// 	return ""; // Wenn kein gültiges Muster gefunden wurde, leere Zeichenfolge zurückgeben
	// }

	// static std::string get_array_constant_base(const std::string& input) {
	// 	size_t pos = input.find_last_of('.');
	// 	if (pos == std::string::npos) {
	// 		return ""; // Kein Punkt gefunden, also kein gültiges Muster
	// 	}
	// 	std::string suffix = input.substr(pos + 1);
	// 	// if array.SIZE constant
	// 	if(suffix.size() == 4 && suffix.substr(0, 4) == "SIZE") {
	// 		return input.substr(0, pos); // Der Teil vor dem Punkt wird zurückgegeben
	// 	}
	// 	return "";
	// }

	// /// returns nullptr if ref was no ndarray constant and could not be substituted
	// /// ndarray.SIZE_D1 -> nd.SIZE_D1
	// NodeAST* substitute_ndarray_constants(NodeReference* ref) {
	// 	// special case when variable ref is ndarray constant
	// 	if(ref->data_type == DataType::Const) {
	// 		const std::string ndarray_name = get_ndarray_constant_base(ref->name);
	// 		if(ndarray_name.empty()) {
	// 			// no ndarray constant pattern found -> search for array constant pattern array.SIZE
	// 			const std::string array_name = get_array_constant_base(ref->name);
	// 			if(array_name.empty()) return nullptr;
	// 			if(const auto substitute = get_substitute(array_name)) {
	// 				if(const auto array_ref = substitute->cast<NodeArrayRef>()) {
	// 					ref->name = array_ref->sanitize_name() + remove_substring(ref->name, array_name);
	// 					ref->ty = TypeRegistry::Integer;
	// 					ref->declaration.reset();
	// 					return ref;
	// 				}
	// 			}
	// 		}
	// 		if(const auto substitute = get_substitute("_"+ndarray_name)) {
	// 			if(const auto array_ref = substitute->cast<NodeArrayRef>()) {
	// 				ref->name = array_ref->sanitize_name() + remove_substring(ref->name, ndarray_name);
	// 				ref->ty = TypeRegistry::Integer;
	// 				ref->declaration.reset();
	// 				return ref;
	// 			}
	// 			// in case it is before datastructure lowering and ndarray refs still exist
	// 		} else if(const auto nd_substitute = get_substitute(ndarray_name)) {
	// 			if(const auto nd_array_ref = nd_substitute->cast<NodeNDArrayRef>()) {
	// 				ref->name = nd_array_ref->name + remove_substring(ref->name, ndarray_name);
	// 				ref->ty = TypeRegistry::Integer;
	// 				ref->declaration.reset();
	// 				return ref;
	// 			}
	// 		}
	// 	}
	// 	return nullptr;
	// }

	/// if substitute and ref are both of type <Composite> and <ArrayRef>: only change name
	static NodeReference* substitute_composite_type(NodeReference* ref, NodeAST* substitute) {
		if(substitute->cast<NodeInitializerList>()) return nullptr;

		if(substitute->ty->cast<CompositeType>()) {
			if (!substitute->cast<NodeArrayRef>() and !substitute->cast<NodeNDArrayRef>()) {
				auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
				error.m_message = "Arg is of type <Composite> but is no <ArrayRef> Node: <" + ref->name + ">.";
				error.exit();
			}
			if(ref->cast<NodeVariableRef>()) {
				auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
				error.m_message = "Tried to substitute a <Variable> function argument with an <Array>";
				error.exit();
			}
			// if substitution is ndarray with wildcards [*, *] -> replace
			// is substitution is array with no index or one wildcard -> continue name change
			// then ref has to have no indexes or also wildcards
			auto nd_substitute = substitute->cast<NodeNDArrayRef>();
			auto ref_array = static_cast<NodeCompositeRef*>(ref);
			if(nd_substitute and nd_substitute->num_wildcards()) {
				if(ref_array->num_wildcards()) {
					return nullptr;
				} else {
					auto error = CompileError(ErrorType::SyntaxError, "", "", ref->tok);
					error.m_message = "Tried to substitute a function argument of type <"+ref->ty->to_string()+"> with "
									   "an <NDArray> containing wildcards and of type "+substitute->ty->to_string()+".";
					error.exit();
				}
			}

			auto array_ref = static_cast<NodeReference*>(substitute);
			ref->name = array_ref->name;
			//if ref (in function) is of type composite -> same type
			if(ref->ty->get_type_kind() == TypeKind::Composite) {
				ref->ty = substitute->ty;
				// if ref has index and is of type basic then -> set to element type
			} else {
				ref->ty = substitute->ty->get_element_type();
			}
			ref->declaration = array_ref->declaration;
			return ref;
		}
		return nullptr;
	}

	static NodeReference* substitute_function_type(NodeReference* ref, NodeAST* substitute) {
		if(substitute->ty->cast<FunctionType>()) {
			if (!substitute->cast<NodeFunctionHeaderRef>() and !ref->cast<NodeFunctionHeaderRef>()) {
				auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
				error.m_message = "Arg is of type <Function> but is no <FunctionHeaderRef> Node: <" + ref->name + ">.";
				error.exit();
			}
			const auto function_subst = substitute->cast<NodeFunctionHeaderRef>();
			const auto function_ref = ref->cast<NodeFunctionHeaderRef>();
			function_ref->name = function_subst->name;
			function_ref->ty = substitute->ty;
			function_ref->declaration = function_subst->declaration;
			return function_ref;
		}
		return nullptr;
	}

	NodeAST* do_substitution(NodeReference* ref) {
		if(m_substitution_stack.empty()) return ref;
		if(ref->get_declaration() and !ref->get_declaration()->is_function_param()) return ref;

		if(auto substitute = get_substitute(ref->name)) {
			// check if substitute will replace l_value of assignment and is Literal
			if(auto assignment = ref->is_l_value()) {
				if (assignment->kind == NodeInstruction::ParameterStack) {
					m_immutable_param_stack_assignments.push_back(assignment);
					return nullptr;
				}
				if(!substitute->ty->cast<CompositeType>() and substitute->is_constant()) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", substitute->tok);
					error.m_message = "Tried to substitute an l_value of an assignment with an immutable value. Left side of assignment must be a reference.";
					error.exit();
				}
			}
			if(ref->is_func_arg()) {
				const auto func_call = ref->parent->parent->parent->cast<NodeFunctionCall>();
				if(func_call->is_destructive_builtin_func()) {
					if(substitute->is_constant() and !substitute->ty->cast<CompositeType>()) {
						auto error = CompileError(ErrorType::TypeError, "", "", ref->tok);
						error.m_message = "Tried to substitute an argument of a destructive builtin function with an immutable value.";
						error.m_message += " Destructive functions require a variable as an argument.";
						error.m_got = substitute->tok.val;
						error.exit();
					}
				}
			}

			// if substitute and ref are both of type <Composite> and <ArrayRef>: only change name
			if(auto composite_substitute = substitute_composite_type(ref, substitute.get())) {
				return composite_substitute;
			} else if(auto function_substitute = substitute_function_type(ref, substitute.get())) {
				return function_substitute;
			} else {
				return ref->replace_with(std::move(substitute));
			}
		// } else if(auto ndarray_substitute = substitute_ndarray_constants(ref)) {
		// 	return ndarray_substitute;
		}
		return ref;
	}

	/// track all parameter->arg pairs when substituting
	std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
	std::unique_ptr<NodeAST> get_substitute(const std::string& name) {
		if(m_substitution_stack.empty()) return nullptr;
		const auto & map = m_substitution_stack.top();
		if(const auto it = map.find(name); it != map.end()) {
			auto substitute = it->second->clone();
			substitute->update_parents(nullptr);
			return substitute;
		}
		return nullptr;
	}

};
