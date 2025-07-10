//
// Created by Mathias Vatter on 27.10.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTPrinter final : public ASTVisitor {
	std::ostringstream os;
	std::string m_indent = "\t";
	int m_scope_count = 0;

public:
    NodeAST * visit(NodeProgram& node) override;
	NodeAST * visit(NodeInt& node) override;
	NodeAST * visit(NodeReal& node) override;
	NodeAST * visit(NodeString& node) override;
	NodeAST * visit(NodeNil& node) override;
	NodeAST * visit(NodeWildcard& node) override;
	NodeAST * visit(NodeReturn& node) override;
	NodeAST * visit(NodeBreak& node) override;
	NodeAST * visit(NodeNumElements& node) override;
	NodeAST * visit(NodeSortSearch& node) override;
	NodeAST * visit(NodePairs& node) override;
	NodeAST * visit(NodeRange& node) override;
	NodeAST * visit(NodeSingleReturn& node) override;
	NodeAST * visit(NodeDelete& node) override;
	NodeAST * visit(NodeSingleDelete& node) override;
	NodeAST * visit(NodeRetain& node) override;
	NodeAST * visit(NodeSingleRetain& node) override;
	NodeAST * visit(NodeVariable& node) override;
	NodeAST * visit(NodeVariableRef& node) override;
	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;
	NodeAST * visit(NodeArray& node) override;
	NodeAST * visit(NodeArrayRef& node) override;
	NodeAST * visit(NodeNDArray& node) override;
	NodeAST * visit(NodeNDArrayRef& node) override;
	NodeAST * visit(NodeUIControl& node) override;
	NodeAST * visit(NodeDeclaration& node) override;
	NodeAST * visit(NodeFunctionParam& node) override;
    NodeAST * visit(NodeSingleDeclaration& node) override;
	NodeAST * visit(NodeReferenceList& node) override;
	NodeAST * visit(NodeParamList& node) override;
	NodeAST * visit(NodeInitializerList& node) override;
	NodeAST * visit(NodeBinaryExpr& node) override;
	NodeAST * visit(NodeUnaryExpr& node) override;
	NodeAST * visit(NodeAssignment& node) override;
    NodeAST * visit(NodeSingleAssignment& node) override;
	NodeAST * visit(NodeConst& node) override;
	NodeAST * visit(NodeStruct& node) override;
	NodeAST * visit(NodeFamily& node) override;
	NodeAST * visit(NodeNamespace& node) override;
    NodeAST * visit(NodeStatement& node) override;
    NodeAST * visit(NodeBlock& node) override;
	NodeAST * visit(NodeIf& node) override;
	NodeAST * visit(NodeWhile& node) override;
	NodeAST * visit(NodeFor& node) override;
	NodeAST * visit(NodeForEach& node) override;
	NodeAST * visit(NodeSelect& node) override;
	NodeAST * visit(NodeCallback& node) override;
	NodeAST * visit(NodeFunctionHeaderRef& node) override;
	NodeAST * visit(NodeFunctionHeader& node) override;
	NodeAST * visit(NodeFunctionCall& node) override;
	NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeGetControl& node) override;
	NodeAST * visit(NodeSetControl& node) override;
	NodeAST * visit(NodeAccessChain& node) override;

    void generate(const std::string& path) {
        std::ofstream outFile(path);
        if (outFile) {
            outFile << os.str();
        } else {
            // Fehlerbehandlung, falls die Datei nicht geöffnet werden kann
            std::cerr << "Fehler beim Öffnen der Datei: " << path << std::endl;
        }
    	os.str("");
    }

	void print() {
        std::cout << os.str();
    	os.str("");
    }

private:
	void add_declaration_info(const NodeReference& node) {
		auto declaration = node.get_declaration();
		if (declaration) {
			os << " {" << declaration->name << "} ";
		} else {
			os << " {no declaration} ";
		}
	}

    std::string get_indent() const {
        std::string result;
        for (int i = 0; i < m_scope_count; i++) {
            result += m_indent;
        }
        return result;
    }


};
