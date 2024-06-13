//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTVariableChecking : public ASTVisitor {
public:
	explicit ASTVariableChecking(DefinitionProvider* definition_provider, bool fail=false);

	void visit(NodeProgram& node) override;
	/// check if on init callback currently
	void visit(NodeCallback& node) override;
	/// Check for correct variable types and parameter number
	void visit(NodeUIControl& node) override;
	/// Scoping
	void visit(NodeBlock& node) override;
    /// decide if declaration is local or global
    void visit(NodeSingleDeclaration& node) override;
	/// Check if correctly declared and save declaration
	void visit(NodeArray& node) override;
    /// get declaration
    void visit(NodeArrayRef& node) override;
	/// Check if correctly declared. Replace with Array when no brackets are used
	void visit(NodeVariable& node) override;
    /// get declaration
    void visit(NodeVariableRef& node) override;
	void visit(NodeNDArray& node) override;
	void visit(NodeNDArrayRef& node) override;
	void visit(NodeList& node) override;
	void visit(NodeListRef& node) override;
	/// handle get_ui_id specific checks. Replace variable parameter when in get_ui_id and not ui_control
	void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionDefinition& node) override;

	void visit(NodeConstStatement& node) override;

private:
	// boolean to continue after not finding declaration or fail
	bool fail = false;

	NodeProgram* m_program = nullptr;
    NodeBlock* m_current_body = nullptr;
	DefinitionProvider* m_def_provider = nullptr;

	/// apply type annotations given before parse time and replace node types accordingly
	/// returns the new datastructure pointer if replaced, or the old one if not
	NodeDataStructure* apply_type_annotations(NodeDataStructure* node);

	/// check if data structure annotations fit with the detected node type if not in func arguments
	static inline Type* check_annotation_with_expected(NodeDataStructure* node, Type* expected) {
		// skip function parameters
		if(node->is_function_param()) return node->ty;
		if(node->ty == TypeRegistry::Unknown) return node->ty;
		if(!node->ty->is_same_type(expected)) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
			error.m_message = "Type Annotation of "+node->name+" does not match expected type kind.";
			error.m_expected =  "<"+expected->get_type_kind_name()+"> Type";
			error.m_got = "<"+node->ty->get_type_kind_name()+"> Type";
			error.exit();
		}
		return nullptr;
	}

	/// checks if given callback id is of type ui_control
	static bool check_callback_id_data_type(NodeAST* callback_id) {
		std::string id_node_type = "<Array>";
		if(callback_id->get_node_type() == NodeType::VariableRef) {
			id_node_type = "<Variable>";
		}
		auto node_reference = static_cast<NodeReference*>(callback_id);

		// check if callback id reference is ui_control
		auto error = CompileError(ErrorType::TypeError, "", "", callback_id->tok);
		if(node_reference->data_type != DataType::UIControl) {
			error.m_message = id_node_type+" needs to be of type <UI Control> to be referenced in <UI Callback>.";
			error.exit();
		} else {
			// var ref is ui control -> check if it is ui_label
			if(node_reference->declaration and node_reference->declaration->parent and node_reference->declaration->parent->get_node_type() == NodeType::UIControl) {
				auto ui_control = static_cast<NodeUIControl*>(node_reference->declaration->parent);
				if(ui_control->name == "ui_label") {
					error.m_message = "<UI Label> cannot be referenced in <UI Callback>.";
					error.exit();
				}
			}
		}
		return true;
	}
};