//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTVariableChecking : public ASTVisitor {
public:
	explicit ASTVariableChecking(DefinitionProvider* definition_provider);

	void visit(NodeProgram& node) override;
	/// check if on init callback currently
	void visit(NodeCallback& node) override;
	/// Check for correct variable types and parameter number
	void visit(NodeUIControl& node) override;
	/// Scoping
	void visit(NodeBody& node) override;
    /// decide if declaration is local or global
    void visit(NodeSingleDeclareStatement& node) override;
	/// Check if correctly declared and save declaration
	void visit(NodeArray& node) override;
    /// get declaration
    void visit(NodeArrayRef& node) override;
	/// Check if correctly declared. Replace with Array when no brackets are used
	void visit(NodeVariable& node) override;
    /// get declaration
    void visit(NodeVariableRef& node) override;
	/// handle get_ui_id specific checks. Replace variable parameter when in get_ui_id and not ui_control
	void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionDefinition& node) override;

private:
	NodeProgram* m_program = nullptr;
	NodeCallback* m_current_callback = nullptr;
    NodeBody* m_current_body = nullptr;
	bool m_is_init_callback = false;
	DefinitionProvider* m_def_provider = nullptr;

    static inline CompileError throw_declaration_error(NodeReference* node) {
        auto compile_error = CompileError(ErrorType::Variable, "","", node->tok);
        std::string type = "<Variable>";
        if(node->get_node_type() == NodeType::Array) type = "<Array>";
        compile_error.m_message = type+" has not been declared: " + node->name+".";
        compile_error.m_expected = "Valid declaration";
        compile_error.m_got = node->name;
        return compile_error;
    };

    static inline CompileError throw_declaration_type_error(NodeReference* node) {
        auto compile_error = CompileError(ErrorType::Variable, "","", node->tok);
        if(!node->declaration) throw_declaration_error(node).exit();
        if(node->declaration->get_node_type() == NodeType::Array && node->get_node_type() == NodeType::Variable) {
            compile_error.m_message = "Incorrect Reference type. Reference was declared as <Array>: " + node->name+".";
            compile_error.m_expected = "<Array>";
            compile_error.m_got = "<Variable>";
            compile_error.exit();
        }
        if(node->declaration->get_node_type() == NodeType::Variable && node->get_node_type() == NodeType::Array) {
            compile_error.m_message = "Incorrect Reference type. Reference was declared as <Variable>: " + node->name+".";
            compile_error.m_expected = "<Variable>";
            compile_error.m_got = "<Array>";
            compile_error.exit();
        }
        return compile_error;
    }
};