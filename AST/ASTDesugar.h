//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include <unordered_map>
#include <type_traits>

template <typename T>
bool is_instance_of(NodeAST* node) {
    static_assert(std::is_base_of<NodeAST, T>::value, "T must be a subclass of NodeAST");
    return dynamic_cast<T*>(node) != nullptr;
}

template <typename T>
T* cast_node(NodeAST* node) {
    static_assert(std::is_base_of<NodeAST, T>::value, "T must be a subclass of NodeAST");
    return dynamic_cast<T*>(node);
}

inline std::vector<std::string> MATH_OPERATORS = {"-", "+", "/", "*", "mod"};

class ASTDesugar : public ASTVisitor {
public:
    ASTDesugar();
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
    std::vector<std::tuple<NodeArray*, NodeParamList*>> m_declared_arrays;
    std::vector<std::unique_ptr<NodeVariable>> m_declared_variables;
    std::stack<std::string> m_prefixes;
    std::stack<std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>>> m_substitution_stack;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
    std::vector<std::string> m_function_call_stack;

    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> get_substitution_vector(NodeFunctionHeader* definition, NodeFunctionHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
    template<typename T>std::unique_ptr<NodeStatement> statement_wrapper(std::unique_ptr<T> node, NodeAST* parent);
    static std::unique_ptr<NodeStatement> make_function_call(const std::string& name, std::vector<std::unique_ptr<NodeAST>> args, NodeAST* parent, Token tok);
    static std::unique_ptr<NodeBinaryExpr> make_binary_expr(ASTType type, const std::string& op, std::unique_ptr<NodeAST> lhs, std::unique_ptr<NodeAST> rhs, NodeAST* parent, Token tok);
    static std::unique_ptr<NodeInt> make_int(int32_t value, NodeAST* parent);
    std::unique_ptr<NodeParamList> make_init_array_list(const std::vector<int32_t>& values, NodeAST* parent);
    std::unique_ptr<NodeStatement> make_declare_array(const std::string& name, int32_t size, const std::vector<int32_t>& values, NodeAST* parent);
    std::unique_ptr<NodeStatement> make_declare_variable(const std::string& name, int32_t value, VarType type, NodeAST* parent);
    std::unique_ptr<NodeFunctionDefinition> get_function_definition(NodeFunctionHeader* function_header);
};

