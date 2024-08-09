//
// Created by Mathias Vatter on 08.08.24.
//

#pragma once

#include <regex>
#include "ASTVisitor.h"

class ASTFunctionInlining : public ASTVisitor {
public:
	explicit ASTFunctionInlining(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

	/// check for used functions
	inline NodeAST *visit(NodeProgram &node) override {
		m_program = &node;
		node.update_function_lookup();

		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		/// vector to house only the definitions that are actually used in the program
		std::vector<std::unique_ptr<NodeFunctionDefinition>> final_function_definitions;
		for(auto & func_def : node.function_definitions) {
			if(m_used_function_definitions.find(func_def.get()) != m_used_function_definitions.end()) {
				final_function_definitions.push_back(std::move(func_def));
			}
		}
		node.function_definitions = std::move(final_function_definitions);
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

		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		if(node.is_call and !node.function->args->params.empty()) {
			error.m_message = "Found incorrect amount of function arguments when using <call>.";
			error.exit();
		}

		node.get_definition(m_program);
		if(node.kind == NodeFunctionCall::Kind::Property) {
			CompileError(ErrorType::InternalError,"Found undefined property function.", "", node.tok).exit();
		}
		if(!node.definition or node.kind == NodeFunctionCall::Kind::Undefined) {
			error.m_message = "Unable to find function definition for <"+node.function->name+">.";
			error.exit();
		}

		if(node.kind == NodeFunctionCall::Kind::Builtin) {
			if(m_restricted_builtin_functions.find(node.function->name) != m_restricted_builtin_functions.end()) {
				if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
					error.m_message = "<"+node.function->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.";
					error.m_got = "<"+m_current_callback->begin_callback+">";
					error.exit();
				}
			}
			return &node;
		}

		check_recursion(node.definition);
		m_program->function_call_stack.push(node.definition);
		m_functions_in_use.insert(node.definition);

		if(node.kind != NodeFunctionCall::Kind::UserDefined) {
			CompileError(ErrorType::InternalError,"Found function that is neither tagged Property, Undefined, Builtin or UserDefined.", "", node.tok).exit();
		}
		// check if FunctionCall is in statement and not assigned or in a condition etc (Handled bei ReturnFunctionRewriting class)
		if(node.parent->get_node_type() != NodeType::Statement) {
			CompileError(ErrorType::InternalError,"Function call was not rewritten and is not within <Statement>.", "", node.tok).exit();
		}

		// check that we are not in init callback and add the function to used_func_def vector if called
		if(m_current_callback != m_program->init_callback) {
			// make function called if it is no params
			if(node.definition->header->args->params.empty()) node.is_call = true;
			if (node.is_call) {
				m_used_function_definitions.insert(node.definition);
				return &node;
			}
		} else if(node.is_call){
			error.m_message = "The usage of <call> keyword is not allowed in the <on init> callback. Automatically removed <call> and inlined function. Consider not using the <call> keyword.";
			error.print();
			node.is_call = false;
		}

		auto node_func_def = clone_as<NodeFunctionDefinition>(node.definition);
		m_substitution_stack.push(get_substitution_map(node_func_def->header.get(), node.function.get()));

		node_func_def->body->accept(*this);

		m_substitution_stack.pop();
		m_functions_in_use.erase(node.definition);
		m_program->function_call_stack.pop();

		return node.replace_with(std::move(node_func_def->body));
	}

	/// do substitution
	inline NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
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
	inline NodeAST *visit(NodeFunctionHeader& node) override {
		node.args->accept(*this);
		// substitution
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
		for(int i= 0; i<definition->args->params.size(); i++) {
			auto &var = definition->args->params[i];
			std::pair<std::string, std::unique_ptr<NodeAST>> pair;
			if(auto node_data_structure = cast_node<NodeDataStructure>(var.get())) {
				pair.first = node_data_structure->name;
			} else {
				auto error = CompileError(ErrorType::SyntaxError, "", definition->tok.line, "<keyword>", var->tok.val,definition->tok.file);
				error.m_message = "Found incorrect parameter definitions in <Function Definition>. Unable to substitute function arguments. Only <Data Structures> can be substituted.";
				error.exit();
			}
			pair.second = std::move(call->args->params[i]);
			substitution_map.insert(std::move(pair));
		}
		return substitution_map;
	}


	/// returns empty string if ndarray constant pattern is not found
	static std::string get_ndarray_constant_base(const std::string& input) {
		std::regex pattern(R"(^(.*)\.SIZE_D\d+$)");
		std::smatch match;
		if (std::regex_search(input, match, pattern)) {
			// Der Teilstring vor dem Muster wird zurückgegeben
			return match[1].str();
		}
		return "";
	}

	/// returns nullptr if ref was no ndarray constant and could not be substituted
	/// ndarray.SIZE_D1 -> nd.SIZE_D1
	NodeAST* substitute_ndarray_constants(NodeReference* ref) {
		// special case when variable ref is ndarray constant
		if(ref->declaration and ref->get_node_type() == NodeType::VariableRef) {
			std::string ndarray_name = get_ndarray_constant_base(ref->name);
			if(ndarray_name.empty()) return nullptr;
			if(auto substitute = get_substitute("_"+ndarray_name)) {
				if(substitute->get_node_type() == NodeType::ArrayRef) {
					auto array_ref = static_cast<NodeArrayRef*>(substitute.get());
					ref->name = array_ref->name + remove_substring(ref->name, ndarray_name);
					return ref;
				}
			// in case it is before datastructure lowering and ndarray refs still exist
			} else if(auto nd_substitute = get_substitute(ndarray_name)) {
				if(nd_substitute->get_node_type() == NodeType::NDArrayRef) {
					auto nd_array_ref = static_cast<NodeNDArrayRef*>(nd_substitute.get());
					ref->name = nd_array_ref->name + remove_substring(ref->name, ndarray_name);
					return ref;
				}
			}
		}
		return nullptr;
	}

	/// if substitute and ref are both of type <Composite> and <ArrayRef>: only change name
	static NodeReference* substitute_composite_type(NodeReference* ref, NodeAST* substitute) {
		if(ref->ty->get_type_kind() == TypeKind::Composite and substitute->ty->get_type_kind() == TypeKind::Composite) {
			if (substitute->get_node_type() != NodeType::ArrayRef and substitute->get_node_type() != NodeType::NDArrayRef) {
				auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
				error.m_message = "Arg is of type <Composite> but is no <ArrayRef> Node: <" + ref->name + ">.";
				error.exit();
			}
			auto array_ref = cast_node<NodeReference>(substitute);
			ref->name = array_ref->name;
			return ref;
		}
		return nullptr;
	}

	NodeAST* do_substitution(NodeReference* ref) {
		if(m_program->function_call_stack.empty()) return ref;
		if(m_substitution_stack.empty()) return ref;

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

	/// set of all function_definitions that are actually used in program
	/// can only house called functions with no params
	std::set<NodeFunctionDefinition*> m_used_function_definitions;

	/// track functions in use to search for recursive calls
	std::unordered_set<NodeFunctionDefinition*> m_functions_in_use;
	/// track all parameter->arg pairs when substituting
	std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
	inline std::unique_ptr<NodeAST> get_substitute(const std::string& name) {
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

	inline bool check_recursion(NodeFunctionDefinition* func) {
		if(m_functions_in_use.find(func) != m_functions_in_use.end()) {
			// recursive function call detected
			auto error = CompileError(ErrorType::SyntaxError, "", "", func->tok);
			error.m_message = "Recursive function call detected. Calling functions inside their definition is not allowed.";
			error.m_got = func->header->name;
			error.exit();
			return true;
		}
		return false;
	}

};
