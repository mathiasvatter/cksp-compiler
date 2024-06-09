//
// Created by Mathias Vatter on 21.11.23.
//

#pragma once

#include <sstream>
#include "../AST/ASTVisitor/ASTVisitor.h"


inline std::string sanitize_dots(const std::string& str) {
	std::string result;
	for (char ch : str) {
		if (ch == '.') {
			result += "__";
		} else {
			result += ch;
		}
	}
	return result;
}


class ASTGenerator : public ASTVisitor {
public:
    void visit(NodeProgram& node) override;
    void visit(NodeInt& node) override;
    void visit(NodeReal& node) override;
	void visit(NodeString& node) override;
    void visit(NodeVariable& node) override;
    void visit(NodeVariableRef& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeArrayRef& node) override;
    void visit(NodeUIControl& node) override;
    void visit(NodeSingleDeclaration& node) override;
    void visit(NodeParamList& node) override;
    void visit(NodeBinaryExpr& node) override;
    void visit(NodeUnaryExpr& node) override;
    void visit(NodeSingleAssignment& node) override;
    void visit(NodeStatement& node) override;
	void visit(NodeBody& node) override;
    void visit(NodeIf& node) override;
    void visit(NodeWhile& node) override;
    void visit(NodeSelect& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeFunctionHeader& node) override;
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionDefinition& node) override;
    void visit(NodeGetControl& node) override;

    std::ostringstream os;


	void generate(const std::string& path) const;
	void print() const;

private:
	std::string m_indent = "  ";
	int m_scope_count = 0;
	inline std::string get_indent() {
		std::string result;
		for (int i = 0; i < m_scope_count; i++) {
			result += m_indent;
		}
		return result;
	}
    static std::string get_compiled_date_time();
};


