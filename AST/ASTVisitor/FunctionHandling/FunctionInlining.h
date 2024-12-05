//
// Created by Mathias Vatter on 08.08.24.
//

#pragma once

#include "../ASTVisitor.h"

class FunctionInlining : public ASTVisitor {
private:

public:
	explicit FunctionInlining(NodeProgram* main) {
		m_program = main;
	}

	NodeAST * inline_function(NodeFunctionCall& call) {
		if(call.is_builtin_kind()) {
			auto error = CompileError(ErrorType::InternalError, "", "", call.tok);
			error.m_message = "Function of builtin kind can not be inlined.";
		}
		auto definition = call.get_definition();
		if(!definition) {
			auto error = CompileError(ErrorType::InternalError, "", "", call.tok);
			error.m_message = "Function call ready to be inlined has to have a definition.";
		}

		// deal here with expression functions
		if(definition->is_expression_function()) {
//			definition->is_used = false;
			auto node_func_body = clone_as<NodeBlock>(definition->body.get());
			m_substitution_stack.push(get_substitution_map(definition->header.get(), call.function.get()));
			node_func_body->accept(*this);

			m_substitution_stack.pop();
			return call.replace_with(get_expression_return(node_func_body.get()));
		};


		// check if FunctionCall is in statement and not assigned or in a condition etc (Handled bei ReturnFunctionRewriting class)
		if(!call.parent->cast<NodeStatement>()) {
			CompileError(ErrorType::InternalError,"Function call was not rewritten and is not within <Statement>.", "", call.tok).exit();
		}

		// inlining process
		// can not be set to false here, because some func calls might be "called" and some not
//			definition->is_used = false;
		auto node_func_body = clone_as<NodeBlock>(definition->body.get());
		m_substitution_stack.push(get_substitution_map(definition->header.get(), call.function.get()));
		node_func_body->accept(*this);
		m_substitution_stack.pop();
		return call.replace_with(std::move(node_func_body));

	}

private:
	static inline std::unique_ptr<NodeAST> get_expression_return(NodeBlock* body) {
		auto stmt = body->statements[0]->statement.get();
		if(stmt->get_node_type() == NodeType::Return) {
			auto ret = static_cast<NodeReturn*>(stmt);
			return std::move(ret->return_variables[0]);
		}
		auto error = CompileError(ErrorType::InternalError, "", "", body->tok);
		error.m_message = "Function is not a return-only function";
		error.exit();
		return nullptr;
	}

	/// do substitution
	inline NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
		return do_substitution(&node);
	}
	/// do substitution
	inline NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		return do_substitution(&node);
	}
	/// do substitution
	inline NodeAST *visit(NodeVariableRef &node) override {
		return do_substitution(&node);
	}
	/// do substitution
	inline NodeAST *visit(NodeFunctionHeaderRef &node) override {
		if(node.args) node.args->accept(*this);
		return do_substitution(&node);
	}

	static std::unordered_map<std::string, std::unique_ptr<NodeAST>> get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeaderRef* call) {
		std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_map;
		substitution_map.reserve(definition->params.size());
		for (size_t i = 0; i < definition->params.size(); ++i) {
			auto& var = definition->get_param(i);
			// Überprüfen, ob var ein NodeDataStructure ist
			if (auto node_data_structure = static_cast<NodeDataStructure*>(var.get())) {
				// Direktes Einfügen in die Map
				substitution_map[node_data_structure->name] = std::move(call->get_arg(i));
			} else {
				auto error = CompileError(ErrorType::SyntaxError, "", definition->tok.line, "<keyword>", var->tok.val, definition->tok.file);
				error.m_message = "Found incorrect parameter definitions in <Function Definition>. Unable to substitute function arguments. Only <Data Structures> can be substituted.";
				error.exit();
			}
		}
		return substitution_map;
	}


	/// returns empty string if ndarray constant pattern is not found
	static std::string get_ndarray_constant_base(const std::string& input) {
		// Finde die Position des letzten Punktes
		size_t pos = input.find_last_of('.');
		if (pos == std::string::npos) {
			return ""; // Kein Punkt gefunden, also kein gültiges Muster
		}

		// Überprüfe, ob der Teil nach dem Punkt dem erwarteten Muster entspricht
		std::string suffix = input.substr(pos + 1);
		if (suffix.size() > 6 && suffix.substr(0, 6) == "SIZE_D" && std::all_of(suffix.begin() + 6, suffix.end(), ::isdigit)) {
			return input.substr(0, pos); // Der Teil vor dem Punkt wird zurückgegeben
		}

		return ""; // Wenn kein gültiges Muster gefunden wurde, leere Zeichenfolge zurückgeben
	}

	static std::string get_array_constant_base(const std::string& input) {
		size_t pos = input.find_last_of('.');
		if (pos == std::string::npos) {
			return ""; // Kein Punkt gefunden, also kein gültiges Muster
		}
		std::string suffix = input.substr(pos + 1);
		// if array.SIZE constant
		if(suffix.size() == 4 && suffix.substr(0, 4) == "SIZE") {
			return input.substr(0, pos); // Der Teil vor dem Punkt wird zurückgegeben
		}
		return "";
	}

	/// returns nullptr if ref was no ndarray constant and could not be substituted
	/// ndarray.SIZE_D1 -> nd.SIZE_D1
	NodeAST* substitute_ndarray_constants(NodeReference* ref) {
		// special case when variable ref is ndarray constant
		if(ref->data_type == DataType::Const) {
			std::string ndarray_name = get_ndarray_constant_base(ref->name);
			if(ndarray_name.empty()) {
				// no ndarray constant pattern found -> search for array constant pattern array.SIZE
				std::string array_name = get_array_constant_base(ref->name);
				if(array_name.empty()) return nullptr;
				if(auto substitute = get_substitute(array_name)) {
					if(auto array_ref = substitute->cast<NodeArrayRef>()) {
						ref->name = array_ref->sanitize_name() + remove_substring(ref->name, array_name);
						ref->ty = TypeRegistry::Integer;
						ref->declaration.reset();
						return ref;
					}
				}
			}
			if(auto substitute = get_substitute("_"+ndarray_name)) {
				if(auto array_ref = substitute->cast<NodeArrayRef>()) {
					ref->name = array_ref->sanitize_name() + remove_substring(ref->name, ndarray_name);
					ref->ty = TypeRegistry::Integer;
					ref->declaration.reset();
					return ref;
				}
				// in case it is before datastructure lowering and ndarray refs still exist
			} else if(auto nd_substitute = get_substitute(ndarray_name)) {
				if(auto nd_array_ref = nd_substitute->cast<NodeNDArrayRef>()) {
					ref->name = nd_array_ref->name + remove_substring(ref->name, ndarray_name);
					ref->ty = TypeRegistry::Integer;
					ref->declaration.reset();
					return ref;
				}
			}
		}
		return nullptr;
	}

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
			auto function_subst = substitute->cast<NodeFunctionHeaderRef>();
			auto function_ref = ref->cast<NodeFunctionHeaderRef>();
			function_ref->name = function_subst->name;
//			function_ref->name = function_subst->name;
			function_ref->ty = substitute->ty;
			function_ref->declaration = function_subst->declaration;
			return function_ref;
		}
		return nullptr;
	}

	NodeAST* do_substitution(NodeReference* ref) {
//		if(m_program->function_call_stack.empty()) return ref;
		if(m_substitution_stack.empty()) return ref;
		if(ref->get_declaration() and !ref->get_declaration()->is_function_param()) return ref;
//		if(ref->data_type != DataType::Param) return ref;

		if(auto substitute = get_substitute(ref->name)) {
			// if substitute and ref are both of type <Composite> and <ArrayRef>: only change name
			if(auto composite_substitute = substitute_composite_type(ref, substitute.get())) {
				return composite_substitute;
			} else if(auto function_substitute = substitute_function_type(ref, substitute.get())) {
				return function_substitute;
			} else {
				return ref->replace_with(std::move(substitute));
			}
		} else if(auto ndarray_substitute = substitute_ndarray_constants(ref)) {
			return ndarray_substitute;
		}
		return ref;
	}

	/// track all parameter->arg pairs when substituting
	std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
	inline std::unique_ptr<NodeAST> get_substitute(const std::string& name) {
//		if(m_program->function_call_stack.empty()) return nullptr;
		if(m_substitution_stack.empty()) return nullptr;
		const auto & map = m_substitution_stack.top();
		auto it = map.find(name);
		if(it != map.end()) {
			auto substitute = it->second->clone();
			substitute->update_parents(nullptr);
			return substitute;
		}
		return nullptr;
	}

};
