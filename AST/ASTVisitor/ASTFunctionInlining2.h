//
// Created by Mathias Vatter on 08.08.24.
//

#pragma once

#include <regex>
#include "ASTVisitor.h"

class ASTFunctionInlining2 : public ASTVisitor {
public:
	explicit ASTFunctionInlining2(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

	/// check for used functions
	NodeAST *visit(NodeProgram &node) override;

	NodeAST *visit(NodeCallback &node) override;
	/// initiating substitution
	inline NodeAST *visit(NodeFunctionCall &node) override {
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
		m_program->function_call_stack.push(node.definition);
		m_functions_in_use.insert(node.definition);
		if(m_functions_in_use.find(node.definition) != m_functions_in_use.end()) {
			// recursive function call detected
			error.m_message = "Recursive function call detected. Calling functions inside their definition is not allowed.";
			error.m_got = node.function->name;
			error.exit();
		}

		// visit header
		node.function->accept(*this);

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

		if(node.kind != NodeFunctionCall::Kind::UserDefined) {
			CompileError(ErrorType::InternalError,"Found function that is neither tagged Property, Undefined, Builtin or UserDefined.", "", node.tok).exit();
		}
		// check if FunctionCall is in statement and not assigned or in a condition etc (Handled bei ReturnFunctionRewriting class)
		if(node.parent->get_node_type() != NodeType::Statement) {
			CompileError(ErrorType::InternalError,"Function call was not rewritten and is not within <Statement>.", "", node.tok).exit();
		}

		auto node_func_def = clone_as<NodeFunctionDefinition>(node.definition);



		m_functions_in_use.erase(m_program->function_call_stack.top());
		m_program->function_call_stack.pop();
	}




	/// do substitution
	NodeAST *visit(NodeArrayRef &node) override;
	/// do substitution
	NodeAST *visit(NodeVariableRef &node) override;
	/// do substitution
	NodeAST *visit(NodeFunctionHeader &node) override;

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

	std::string get_ndarray_base(NodeReference* ref) {
		// special case when variable ref is ndarray constant
		if(ref->declaration and ref->get_node_type() == NodeType::VariableRef) {
			if(ref->declaration->get_node_type() == NodeType::Array) {
				auto array = static_cast<NodeArray*>(ref->declaration);
				std::string string_pattern = "^_" + array->name + R"(.SIZE_D(\d+)$)";
				std::regex pattern(string_pattern);
				std::smatch match;
				// Überprüfen, ob der String dem Muster entspricht
				if(std::regex_match(ref->name, match, pattern)) {
					// Extrahiere die Zahl aus dem Match
//					int number = std::stoi(match[1].str());
					return "_"+array->name;
				}
			}
		}
		return "";
	}

	NodeAST* do_substitution(NodeReference* ref) {
		if(m_substitution_stack.empty()) return ref;

		if(auto substitute = get_substitute(ref->name)) {
			return ref->replace_with(std::move(substitute));
		} else {
			// special case when variable ref is ndarray constant
			auto ndarray_base = get_ndarray_base(ref);
			if(!ndarray_base.empty()) {
				substitute = get_substitute(ndarray_base);
				if(substitute) {
					auto array_ref = clone_as<NodeArrayRef>(ref);
					array_ref->name = substitute->get_string();
					return ref->replace_with(std::move(array_ref));
				}
			}

			auto error = CompileError(ErrorType::InternalError, "", "", ref->tok);
			error.m_message = "Unable to find substitute for <"+ref->name+">.";
			error.exit();
		}
		return ref;
	}


private:
	DefinitionProvider *m_def_provider;
	NodeCallback* m_current_callback = nullptr;

	/// track functions in use to search for recursive calls
	std::unordered_set<NodeFunctionDefinition*> m_functions_in_use;
	/// track all parameter->arg pairs when substituting
	std::stack<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_substitution_stack;
	std::unique_ptr<NodeAST> get_substitute(const std::string& name) {
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