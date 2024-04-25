//
// Created by Mathias Vatter on 08.12.23.
//

#pragma once


#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTVariables : public ASTVisitor {
public:
    ASTVariables(DefinitionProvider* definition_provider, std::unordered_map<NodeAST *, std::unique_ptr<NodeStatement>> m_function_inlines);

    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeFunctionCall& node) override;

    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;

    void visit(NodeStatement& node) override;

    void visit(NodeBody& node) override;
	void visit(NodeUIControl& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;
    void visit(NodeParamList& node) override;

private:
	DefinitionProvider* m_def_provider;
    std::unordered_map<NodeAST *, std::unique_ptr<NodeStatement>> m_function_inlines;

    /// declared variables
    std::unordered_map<std::string, NodeVariable*, StringHash, StringEqual> m_declared_variables;
    NodeVariable* get_declared_variable(const std::string& var);
	int m_variables_declared = 0;
    /// declared arrays
	std::unordered_map<std::string, NodeArray*, StringHash, StringEqual> m_declared_arrays;
    NodeArray* get_declared_array(const std::string& arr);
    /// declared ui_controls
	std::unordered_map<std::string, NodeUIControl*, StringHash, StringEqual> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);

    /// multidimensional array method for getting the index at runtime
    std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);

};


