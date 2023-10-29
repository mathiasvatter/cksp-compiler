//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include <unordered_map>

inline std::vector<std::string> MATH_OPERATORS = {"-", "+", "/", "*", "mod"};

class ASTDesugar : public ASTVisitor {
public:
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;

    /// turn into single assign statements
	void visit(NodeAssignStatement& node) override;
    /// turn into single declare statements
    void visit(NodeDeclareStatement& node) override;
    void visit(NodeConstStatement& node) override;
	/// alter for loops to while loops
	void visit(NodeForStatement& node) override;

private:

    static std::unique_ptr<NodeStatement> make_function_call(const std::string& name, std::vector<std::unique_ptr<NodeAST>> args, NodeAST* parent, Token tok);
    std::unique_ptr<NodeBinaryExpr> make_binary_expr(ASTType type, const std::string& op, std::unique_ptr<NodeAST> lhs, std::unique_ptr<NodeAST> rhs, NodeAST* parent, Token tok);
    std::unique_ptr<NodeInt> make_int(int32_t value, NodeAST* parent);
    std::unique_ptr<NodeParamList> make_init_array_list(const std::vector<int32_t>& values, NodeAST* parent);
    std::unique_ptr<NodeStatement> make_declare_array(const std::string& name, int32_t size, const std::vector<int32_t>& values, NodeAST* parent);
    std::unique_ptr<NodeStatement> make_declare_variable(const std::string& name, int32_t value, VarType type, NodeAST* parent);

};

