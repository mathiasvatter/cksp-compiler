//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include <unordered_map>
#include <type_traits>

inline std::vector<std::string> MATH_OPERATORS = {"-", "+", "/", "*", "mod"};

class ASTDesugar : public ASTVisitor {
public:
    explicit ASTDesugar(const std::vector<std::unique_ptr<NodeFunctionHeader>> &mBuiltinFunctions);

    /// check if init callback exists
    void visit(NodeProgram& node) override;
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;
    /// initiating substitution
    void visit(NodeFunctionCall& node) override;
//    void visit(NodeFunctionDefinition& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;

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

    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:
    const std::vector<std::unique_ptr<NodeFunctionHeader>>& m_builtin_functions;
    bool is_builtin_function(NodeFunctionHeader* function);

    bool m_in_init_callback;

    std::vector<std::tuple<NodeArray*, NodeParamList*>> m_declared_arrays;
    std::vector<std::unique_ptr<NodeVariable>> m_declared_variables;
    std::stack<std::string> m_prefixes;
    std::stack<std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>>> m_substitution_stack;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
    std::vector<std::string> m_function_call_stack;

    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> get_substitution_vector(NodeFunctionHeader* definition, NodeFunctionHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
    std::unique_ptr<NodeFunctionDefinition> get_function_definition(NodeFunctionHeader* function_header);
    void handle_function_overrides();
};

