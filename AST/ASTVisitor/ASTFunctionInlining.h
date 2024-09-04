//
// Created by Mathias Vatter on 08.08.24.
//

#pragma once

#include <future>
#include "ASTVisitor.h"

class ASTFunctionInlining : public ASTVisitor {
private:
	std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
	std::vector<NodeFunctionCall*> m_function_calls;
public:
	explicit ASTFunctionInlining(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

	/// check for used functions
	inline NodeAST *visit(NodeProgram &node) override {
		m_program = &node;
		node.update_function_lookup();
		node.reset_function_used_flag();
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

//		// Verbesserung: Parallelisiere die Verarbeitung
//		std::vector<std::future<void>> futures;
//		for(auto & callback : node.callbacks) {
//			futures.push_back(std::async(std::launch::async, [&]{
//			  callback->accept(*this);
//			}));
//		}
//
//		for (auto &fut : futures) {
//			fut.get();  // Warte auf die Beendigung aller Aufgaben
//		}

		/// vector to house only the definitions that are actually used in the program
		node.function_definitions = std::move(m_function_definitions);
		node.update_function_lookup();
		for(auto & call : m_function_calls) {
			call->definition = nullptr;
		}
		return &node;
	}

	inline NodeAST *visit(NodeCallback &node) override {
		// empty the local var stack after init, because the idx can be reused now
		m_current_callback = &node;
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		return &node;
	}

	/// initiating substitution
	inline NodeAST *visit(NodeFunctionCall &node) override {
		// visit header
		node.function->accept(*this);

		if(node.is_call and !node.function->args->params.empty()) {
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Found incorrect amount of function arguments when using <call>.";
			error.exit();
		}

		node.get_definition(m_program, true);
		if(node.kind == NodeFunctionCall::Kind::Property) {
			CompileError(ErrorType::InternalError,"Found undefined property function.", "", node.tok).exit();
		}
		if(!node.definition or node.kind == NodeFunctionCall::Kind::Undefined) {
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Unable to find function definition for <"+node.function->name+">.";
			error.exit();
		}

		if(node.kind == NodeFunctionCall::Kind::Builtin) return &node;

		if(node.definition->is_restricted) {
			if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "<"+node.function->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.";
				error.m_got = "<"+m_current_callback->begin_callback+">";
				error.exit();
			}
		}

		// only threadsafe functions can be called in <on init> callback
		if(m_current_callback == m_program->init_callback) {
			if(!node.definition->is_thread_safe) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "Only threadsafe functions can be called in the <on init> callback. Function <"
								  +node.function->name+"> contains asychronous operations.";
				error.exit();
			}
		}

		m_program->function_call_stack.push(node.definition);

		if(node.kind != NodeFunctionCall::Kind::UserDefined) {
			CompileError(ErrorType::InternalError,"Found function that is neither tagged Property, Undefined, Builtin or UserDefined.", "", node.tok).exit();
		}
		// check if FunctionCall is in statement and not assigned or in a condition etc (Handled bei ReturnFunctionRewriting class)
		if(node.parent->get_node_type() != NodeType::Statement) {
			CompileError(ErrorType::InternalError,"Function call was not rewritten and is not within <Statement>.", "", node.tok).exit();
		}

		// check that we are not in init callback
		if(m_current_callback != m_program->init_callback) {
			// make function called if it is no params
//			if(node.definition->header->args->params.empty()) node.is_call = true;

		} else if(node.is_call){
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "The usage of <call> keyword is not allowed in the <on init> callback. Automatically removed <call> and inlined function. Consider not using the <call> keyword.";
			error.print();
			node.is_call = false;
		}


		// visit everything beforehand to get depth first search
		if(!node.definition->visited)
			node.definition->accept(*this);

		std::unique_ptr<NodeFunctionDefinition> node_func_def = nullptr;
		if(node.is_call) {
			m_function_calls.push_back(&node);
//			if (!node.definition->is_used){
			if(!node.definition->visited) {
				m_function_definitions.push_back(clone_as<NodeFunctionDefinition>(node.definition));
				node.definition->call_sites.clear();
				node.definition->is_used = true;
				node.definition->visited = true;
				node.definition = nullptr;
//			}
			}
		} else {
			node_func_def = clone_as<NodeFunctionDefinition>(node.definition);
			m_substitution_stack.push(get_substitution_map(node_func_def->header.get(), node.function.get()));
			node_func_def->body->accept(*this);
			m_substitution_stack.pop();
		}
		m_program->function_call_stack.pop();

		if(node.is_call) {
			return &node;
		} else {
			return node.replace_with(std::move(node_func_def->body));
		}
	}

	/// do substitution
	inline NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
//		mark_function_param_ref(&node);
		return do_substitution(&node);
	}
	/// do substitution
	inline NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
//		mark_function_param_ref(&node);
		return do_substitution(&node);
	}
	/// do substitution
	inline NodeAST *visit(NodeVariableRef &node) override {
//		mark_function_param_ref(&node);
		return do_substitution(&node);
	}
	/// do substitution
	inline NodeAST *visit(NodeFunctionHeader& node) override {
		node.args->accept(*this);
		// substitution
		if(m_program->function_call_stack.empty()) return &node;
		if (!m_substitution_stack.empty()) {
			if (auto substitute = get_substitute(node.name)) {
				if(auto substitute_ref = cast_node<NodeReference>(substitute.get())) {
					node.name = substitute_ref->name;
					return &node;;
				} else {
					auto error = CompileError(ErrorType::SyntaxError, "Cannot substitute Function name with <non-datastructure>", "", node.tok);
					error.m_expected = "<Reference>";
					error.m_got = substitute->get_string();
					error.exit();
				}
			}
		}
		return &node;
	}

	static std::unordered_map<std::string, std::unique_ptr<NodeAST>> get_substitution_map(NodeFunctionHeader* definition, NodeFunctionHeader* call) {
		std::unordered_map<std::string, std::unique_ptr<NodeAST>> substitution_map;
		substitution_map.reserve(definition->args->params.size());
		for (size_t i = 0; i < definition->args->params.size(); ++i) {
			auto& var = definition->args->params[i];
			// Überprüfen, ob var ein NodeDataStructure ist
			if (auto node_data_structure = static_cast<NodeDataStructure*>(var.get())) {
				// Direktes Einfügen in die Map
				substitution_map[node_data_structure->name] = std::move(call->args->params[i]);
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

	/// returns nullptr if ref was no ndarray constant and could not be substituted
	/// ndarray.SIZE_D1 -> nd.SIZE_D1
	NodeAST* substitute_ndarray_constants(NodeReference* ref) {
		// special case when variable ref is ndarray constant
		if(ref->data_type == DataType::Const) {
			std::string ndarray_name = get_ndarray_constant_base(ref->name);
			if(ndarray_name.empty()) return nullptr;
			if(auto substitute = get_substitute("_"+ndarray_name)) {
				if(substitute->get_node_type() == NodeType::ArrayRef) {
					auto array_ref = static_cast<NodeArrayRef*>(substitute.get());
					ref->name = array_ref->sanitize_name() + remove_substring(ref->name, ndarray_name);
					ref->ty = TypeRegistry::Integer;
					ref->declaration = nullptr;
					return ref;
				}
			// in case it is before datastructure lowering and ndarray refs still exist
			} else if(auto nd_substitute = get_substitute(ndarray_name)) {
				if(nd_substitute->get_node_type() == NodeType::NDArrayRef) {
					auto nd_array_ref = static_cast<NodeNDArrayRef*>(nd_substitute.get());
					ref->name = nd_array_ref->name + remove_substring(ref->name, ndarray_name);
					ref->ty = TypeRegistry::Integer;
					ref->declaration = nullptr;
					return ref;
				}
			}
		}
		return nullptr;
	}

	/// if substitute and ref are both of type <Composite> and <ArrayRef>: only change name
	static NodeReference* substitute_composite_type(NodeReference* ref, NodeAST* substitute) {
		if(substitute->ty->get_type_kind() == TypeKind::Composite) {// || ref->ty->get_type_kind() == TypeKind::Composite) {
			if (substitute->get_node_type() != NodeType::ArrayRef and substitute->get_node_type() != NodeType::NDArrayRef) {
				auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
				error.m_message = "Arg is of type <Composite> but is no <ArrayRef> Node: <" + ref->name + ">.";
				error.exit();
			}
			if(ref->get_node_type() == NodeType::VariableRef) {
				auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
				error.m_message = "Tried to substitute a <Variable> function argument with an <Array>";
				error.exit();
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

			return ref;
		}
		return nullptr;
	}

	NodeAST* do_substitution(NodeReference* ref) {
		if(m_program->function_call_stack.empty()) return ref;
		if(m_substitution_stack.empty()) return ref;
//		if(!ref->declaration->is_function_param()) return ref;
//		if(ref->data_type != DataType::Param) return ref;
		if(ref->name == "UI_array.SIZE_D2") {

		}
		if(auto substitute = get_substitute(ref->name)) {
			// if substitute and ref are both of type <Composite> and <ArrayRef>: only change name
			if(auto composite_substitute = substitute_composite_type(ref, substitute.get())) {
				return composite_substitute;
			} else {
				return ref->replace_with(std::move(substitute));
			}
		} else if(auto ndarray_substitute = substitute_ndarray_constants(ref)) {
			return ndarray_substitute;
		}
		return ref;
	}


protected:
	DefinitionProvider *m_def_provider;
	NodeCallback* m_current_callback = nullptr;

//	std::stack<std::unordered_set<std::string>> m_function_params;
//	inline bool mark_function_param_ref(NodeReference* ref) {
//		if(m_program->function_call_stack.empty()) return false;
//		if(m_function_params.empty()) return false;
//		const auto & set = m_function_params.top();
//		auto it = set.find(ref->name);
//		if(it != set.end()) {
//			ref->data_type = DataType::Param;
//			return true;
//		}
//		return false;
//	}

	/// vector of all function_definitions that are actually used in program
	/// can only house called functions with no params
//	std::set<NodeFunctionDefinition*> m_used_function_definitions;

	/// track all parameter->arg pairs when substituting
	std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
	inline std::unique_ptr<NodeAST> get_substitute(const std::string& name) {
		if(m_program->function_call_stack.empty()) return nullptr;
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
