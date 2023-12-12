//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include <unordered_map>
#include <type_traits>


class ASTDesugar : public ASTVisitor {
public:
    ASTDesugar(const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables, const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
               const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_property_functions, const std::vector<std::unique_ptr<NodeUIControl>> &m_builtin_widgets);

    /// check if init callback exists
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;
    /// initiating substitution
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionHeader& node) override;
//    void visit(NodeFunctionDefinition& node) override;
	void visit(NodeUIControl& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeParamList& node) override;

    void visit(NodeGetControlStatement& node) override;
    /// turn into single assign statements
	void visit(NodeAssignStatement& node) override;
    /// turn into single declare statements
    void visit(NodeDeclareStatement& node) override;
    /// replace const block with single declare statements
    void visit(NodeConstStatement& node) override;
    /// replace family block with single declare statements
    void visit(NodeFamilyStatement& node) override;
	/// alter for loops to while loops
    void visit(NodeForStatement& node) override;
    void visit(NodeListStatement& node) override;
    void visit(NodeStatementList& node) override;

    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:
    const std::vector<std::unique_ptr<NodeFunctionHeader>>& m_builtin_functions;
    NodeFunctionHeader* get_builtin_function(NodeFunctionHeader* function);
    NodeFunctionHeader* get_builtin_function(const std::string &function);

	const std::vector<std::unique_ptr<NodeUIControl>>& m_builtin_widgets;
	NodeUIControl* get_builtin_widget(const std::string &ui_control);

    std::vector<std::string> m_compiler_variables = {"$list_it", "$ui_array_it", "$string_it"};
    void declare_compiler_variables();

    const std::vector<std::unique_ptr<NodeVariable>>& m_builtin_variables;
    std::unique_ptr<NodeVariable> shorthand_to_control_param(const std::string& shorthand);
    // returns either string (for get/set_control_par_str) or integer (for get/set_control_par)
    static ASTType get_control_function_type(const std::string& control_param);

    const std::vector<std::unique_ptr<NodeFunctionHeader>>& m_property_functions;
    NodeFunctionHeader* get_property_function(NodeFunctionHeader* function);
    std::unique_ptr<NodeStatementList> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header);

    bool m_in_init_callback = false;
    NodeCallback* m_init_callback;
    NodeCallback* m_current_callback;
    bool m_processing_function = false;

    std::vector<std::tuple<NodeArray*, NodeParamList*>> m_declared_arrays;
    std::vector<std::unique_ptr<NodeVariable>> m_declared_variables;
    std::stack<std::string> m_family_prefixes;
    std::stack<std::string> m_const_prefixes;
    std::stack<std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>>> m_substitution_stack;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
    std::vector<std::string> m_function_call_stack;
    std::vector<std::unique_ptr<NodeStatement>> m_declare_statements_to_move;

    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> get_substitution_vector(NodeFunctionHeader* definition, NodeFunctionHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
    std::unique_ptr<NodeFunctionDefinition> get_function_definition(NodeFunctionHeader* function_header);

    std::vector<std::unique_ptr<NodeStatement>> add_read_functions(NodeAST* var, NodeAST* parent);
    /// multidimensional array method for getting the size at declaration time
    std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok);

};

