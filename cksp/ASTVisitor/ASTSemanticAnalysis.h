//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"

/**
 * This class completes ASTNodes like arrays, UI controls, etc., by filling in declaration information.
 * It also checks for uniqueness of callback names and existence of "on init" callback.
 * It adds globally declared data structures to the start of the program.
 * It tracks data structure definitions with DefinitionProvider and sets scopes for body nodes.
 * It adds scope to the following:
 * - if-statement
 * - select-statement
 * - while-statement
 * - function definition
 * - callback
 * Changes node types of function parameters at call sites regarding the function definition.
 * Changes node types of incorrectly detected data structures and references.
 */
class ASTSemanticAnalysis final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

	/// Whether <obj[i]> here may be a call to an overloaded subscript rather than an array
	/// element.
	///
	/// An object declared from its constructor has no type yet in this pass - that is what
	/// TypeInference is for - so the overload cannot always be found here. What can be seen is
	/// that the declaration is no array, and a subscript on something that is no array is
	/// either the overload or an error TypeInference reports with the types in hand. Either
	/// way this pass has nothing left to say about it.
	[[nodiscard]] bool may_be_overloaded_subscript(const NodeReference& node) const {
		const auto declaration = node.get_declaration();
		if (!m_program or !declaration) return false;
		if (m_program->find_subscript_overload(node, token::GET_ITEM)
			or m_program->find_subscript_overload(node, token::SET_ITEM)) {
			return true;
		}
		return declaration->ty == TypeRegistry::Unknown
			and declaration->get_node_type() != NodeType::NDArray
			and declaration->get_node_type() != NodeType::Array
			and declaration->get_node_type() != NodeType::List;
	}

public:
	explicit ASTSemanticAnalysis(NodeProgram* main);

    NodeAST * visit(NodeProgram& node) override;
	NodeAST * visit(NodeCallback& node) override;

	NodeAST * visit(NodeBlock& node) override;
	NodeAST * visit(NodeSingleDeclaration& node) override;
	NodeAST * visit(NodeSingleAssignment& node) override;

	NodeAST * visit(NodeArray& node) override;
    NodeAST * visit(NodeArrayRef& node) override;
	NodeAST * visit(NodeString& node) override;

	NodeAST * visit(NodeNDArray& node) override;
    NodeAST * visit(NodeNDArrayRef& node) override;

	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;

	/// List Struct and Reference
	NodeAST * visit(NodeList& node) override;
	NodeAST * visit(NodeListRef& node) override;
	/// Variable
	NodeAST * visit(NodeVariable& node) override;
    NodeAST * visit(NodeVariableRef& node) override;

	NodeAST * visit(NodeFunctionHeaderRef& node) override;
	/// provide and search for function definition; handle is_thread_safe flag
	NodeAST * visit(NodeFunctionCall& node) override;
    /// add function parameters to scope
	NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeWildcard& node) override;
	NodeAST * visit(NodeMemberPath& node) override;
	NodeAST * visit(NodeArrayQuery& node) override;
	NodeAST * visit(NodeNumElements& node) override;
	NodeAST * visit(NodeSortSearch& node) override;
	NodeAST * visit(NodeRange& node) override;
	NodeAST * visit(NodePairs& node) override;

	/// updates the node types of parameters at call sites regarding the function definition
	/// e.g. params can be incorrectly detected as variable refs at call sites, but they are arrays in the definition
	void update_func_call_node_types(const NodeFunctionCall *func_call);
	/// updates incorrectly detected function params (eg arrays detected as variables)
	static NodeDataStructure* replace_incorrectly_detected_data_struct(const std::shared_ptr<NodeDataStructure>& data_struct);
	/// updated incorrectly detected references of function params
	static NodeReference* replace_incorrectly_detected_reference(NodeReference* reference);

	/// warns when a pass-by-value function parameter is modified in the function body,
	/// once per parameter. The modification is local and lost at the call site
	void check_param_modification(NodeReference& ref);

	/// track functions in use to search for recursive calls
	std::unordered_set<NodeFunctionDefinition*> m_functions_in_use{};
	/// parameters already warned about being modified while passed by value
	std::unordered_set<const NodeFunctionParam*> m_warned_params{};
	bool check_recursion(NodeFunctionDefinition* func) const {
		if(m_functions_in_use.contains(func)) {
			// recursive function call detected
			auto error = Diagnostic(ErrorType::SyntaxError, "", "", func->tok);
			error.message = "Found recursive function call <"+func->header->name+">. Calling functions inside their definition is not allowed.";
			error.actual = "Function cycle with: ";
			for (const auto fun : m_functions_in_use) {
				error.actual += "<"+fun->header->name+">, ";
			}
			error.actual.erase(error.actual.size() - 2);
			diagnostics().fatal(std::move(error));
			return true;
		}
		return false;
	}

};
