//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTPrinter : public ASTVisitor {
public:
    void visit(NodeProgram& node) override;
	void visit(NodeInt& node) override;
	void visit(NodeReal& node) override;
	void visit(NodeString& node) override;
	void visit(NodeNil& node) override;
	void visit(NodeReturn& node) override;
	void visit(NodeVariable& node) override;
	void visit(NodeVariableRef& node) override;
	void visit(NodePointer& node) override;
	void visit(NodePointerRef& node) override;
	void visit(NodeArray& node) override;
	void visit(NodeArrayRef& node) override;
	void visit(NodeUIControl& node) override;
	void visit(NodeDeclaration& node) override;
    void visit(NodeSingleDeclaration& node) override;
	void visit(NodeParamList& node) override;
	void visit(NodeBinaryExpr& node) override;
	void visit(NodeUnaryExpr& node) override;
	void visit(NodeAssignment& node) override;
    void visit(NodeSingleAssignment& node) override;
	void visit(NodeConstBlock& node) override;
	void visit(NodeStruct& node) override;
	void visit(NodeFamily& node) override;
    void visit(NodeStatement& node) override;
    void visit(NodeBlock& node) override;
	void visit(NodeIf& node) override;
	void visit(NodeWhile& node) override;
	void visit(NodeFor& node) override;
	void visit(NodeSelect& node) override;
	void visit(NodeCallback& node) override;
	void visit(NodeFunctionHeader& node) override;
	void visit(NodeFunctionCall& node) override;
	void visit(NodeFunctionDefinition& node) override;
	void visit(NodeGetControl& node) override;

    inline void generate(const std::string& path) const {
        std::ofstream outFile(path);
        if (outFile) {
            outFile << os.str();
        } else {
            // Fehlerbehandlung, falls die Datei nicht geöffnet werden kann
            std::cerr << "Fehler beim Öffnen der Datei: " << path << std::endl;
        }
    }

    inline void print() const {
        std::cout << os.str();
    }
private:
    std::ostringstream os;
    std::string m_indent = "\t";
    int m_scope_count = 0;
    inline std::string get_indent() {
        std::string result;
        for (int i = 0; i < m_scope_count; i++) {
            result += m_indent;
        }
        return result;
    }
};
