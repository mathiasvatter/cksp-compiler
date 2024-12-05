
//
// Created by Mathias Vatter on 21.11.23.
//

#pragma once

#include <sstream>
#include "../AST/ASTVisitor/ASTVisitor.h"


inline std::string sanitize_dots(const std::string& str) {
	std::string result = str;
	size_t pos = 0;

	// Ersetze "::" durch "__"
	while ((pos = result.find("::", pos)) != std::string::npos) {
		result.replace(pos, 2, "__");
		pos += 2;  // Gehe zur nächsten Position nach dem Ersetzen
	}

	pos = 0;
	// Ersetze "." durch "__"
	while ((pos = result.find('.', pos)) != std::string::npos) {
		result.replace(pos, 1, "__");
		pos += 2;  // Gehe zur nächsten Position nach dem Ersetzen
	}

	return result;
}


class ASTGenerator : public ASTVisitor {
public:
    NodeAST * visit(NodeProgram& node) override;
    NodeAST * visit(NodeInt& node) override;
    NodeAST * visit(NodeReal& node) override;
	NodeAST * visit(NodeString& node) override;
    NodeAST * visit(NodeVariable& node) override;
    NodeAST * visit(NodeVariableRef& node) override;
	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;
    NodeAST * visit(NodeArray& node) override;
    NodeAST * visit(NodeArrayRef& node) override;
    NodeAST * visit(NodeUIControl& node) override;
    NodeAST * visit(NodeSingleDeclaration& node) override;
    NodeAST * visit(NodeParamList& node) override;
	NodeAST * visit(NodeInitializerList& node) override;
    NodeAST * visit(NodeBinaryExpr& node) override;
    NodeAST * visit(NodeUnaryExpr& node) override;
    NodeAST * visit(NodeSingleAssignment& node) override;
    NodeAST * visit(NodeStatement& node) override;
	NodeAST * visit(NodeBlock& node) override;
    NodeAST * visit(NodeIf& node) override;
    NodeAST * visit(NodeWhile& node) override;
    NodeAST * visit(NodeSelect& node) override;
    NodeAST * visit(NodeCallback& node) override;
	NodeAST * visit(NodeFunctionHeaderRef& node) override;
	NodeAST * visit(NodeFunctionHeader& node) override;
    NodeAST * visit(NodeFunctionCall& node) override;
    NodeAST * visit(NodeFunctionDefinition& node) override;
    NodeAST * visit(NodeGetControl& node) override;
	NodeAST * visit(NodeNumElements& node) override;
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


