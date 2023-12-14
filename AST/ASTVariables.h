//
// Created by Mathias Vatter on 08.12.23.
//

#pragma once


#include "ASTVisitor.h"

// user-defined hash functions for comparison and unordered_map
struct StringHash {
	size_t operator()(const std::string& key) const {
		std::string lower_key;
		std::transform(key.begin(), key.end(), std::back_inserter(lower_key), [](unsigned char c){ return std::tolower(c); });
		return std::hash<std::string>()(key);
	}
};

struct StringEqual {
	bool operator()(const std::string& a, const std::string& b) const {
		return string_compare(a, b);
//		return a == b;
	}
};



class ASTVariables : public ASTVisitor {
public:
    ASTVariables(const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
                  const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables,
                  const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays,
				 const std::vector<std::unique_ptr<NodeUIControl>> &m_builtin_widgets);

    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeFunctionCall& node) override;

    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;


    void visit(NodeStatementList& node) override;
	void visit(NodeUIControl& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:

	/// builtin engine functions
    const std::vector<std::unique_ptr<NodeFunctionHeader>>& m_builtin_functions;
    NodeFunctionHeader* get_builtin_function(const std::string &function);
    /// builtin engine variables
    const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables;
    NodeVariable* get_builtin_variable(NodeVariable* var);
    /// builtin engine arrays
    const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays;
    NodeArray* get_builtin_array(NodeArray* arr);
	/// builtin engine widgets
	const std::vector<std::unique_ptr<NodeUIControl>> &m_builtin_widgets;
	NodeUIControl* get_builtin_widget(const std::string &ui_control);

    /// declared variables
//    std::vector<NodeVariable*> m_declared_variables;
    std::unordered_map<std::string, NodeVariable*, StringHash, StringEqual> m_declared_variables;
    NodeVariable* get_declared_variable(const std::string& var);
	int m_variables_declared = 0;
    /// declared arrays
//    std::vector<NodeArray*> m_declared_arrays;
	std::unordered_map<std::string, NodeArray*, StringHash, StringEqual> m_declared_arrays;
    NodeArray* get_declared_array(const std::string& arr);
    /// declared ui_controls
//    std::vector<NodeUIControl*> m_declared_controls;
	std::unordered_map<std::string, NodeUIControl*, StringHash, StringEqual> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);

    /// multidimensional array method for getting the index at runtime
    std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);

};


