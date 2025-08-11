//
// Created by Mathias Vatter on 02.08.25.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTPrinter final : public PreASTVisitor {
	std::ostringstream os;
	std::string m_indent = "\t";
	int m_scope_count = 0;

public:
	// explicit PreASTPrinter(PreNodeProgram* program) : PreASTVisitor(program) {}
	// ~PreASTPrinter() override = default;

	PreNodeAST *visit(PreNodeNumber &node) override {
	    os << node.tok.val;
		return &node;
	}

	PreNodeAST *visit(PreNodeInt &node) override {
        os << node.integer;
		return &node;
    }

	PreNodeAST *visit(PreNodeUnaryExpr &node) override {
	    os << get_token_string(node.op);
        node.operand->accept(*this);
		return &node;
    }

	PreNodeAST *visit(PreNodeBinaryExpr &node) override {
        node.left->accept(*this);
	    os << " " << get_token_string(node.op) << " ";
        node.right->accept(*this);
		return &node;
    }

	PreNodeAST *visit(PreNodeKeyword &node) override {
	    os << node.tok.val;
		return &node;
    }

	PreNodeAST *visit(PreNodeOther &node) override {
	    os << node.tok.val;
		return &node;
    }

	PreNodeAST *visit(PreNodeDeadCode &node) override {
        os << node.tok.val;
		return &node;
	}

	PreNodeAST *visit(PreNodePragma &node) override {
        node.option->accept(*this);
        node.argument->accept(*this);
		return &node;
    }

	PreNodeAST *visit(PreNodeStatement &node) override {

		os << get_indent();
        node.statement->accept(*this);
		return &node;
    }

	PreNodeAST *visit(PreNodeChunk &node) override {
		m_scope_count++;
	    visit_all(node.chunk, *this);
		m_scope_count--;
		return &node;
    }

	PreNodeAST *visit(PreNodeDefineHeader &node) override {
	    os << node.name->accept(*this);
		os << "(";
        node.args->accept(*this);
		os << ")";
		return &node;
    }

	PreNodeAST *visit(PreNodeList &node) override {
		for (auto& param : node.params) {
			param->accept(*this);
			if (&param != &node.params.back()) {
				os << ", ";
			}
		}
		return &node;
    }

	PreNodeAST *visit(PreNodeDefineStatement &node) override {
		os << "define ";
        node.header->accept(*this);
		os << " := ";
        node.body->accept(*this);
		os << "\n";
		return &node;
    }

	PreNodeAST *visit(PreNodeDefineCall &node) override {
        node.define->accept(*this);
		os << "()";
		return &node;
    }

	PreNodeAST *visit(PreNodeProgram &node) override {
	    visit_all(node.define_statements, *this);
		os << "\n";
	    visit_all(node.macro_definitions, *this);
		os << "\n";
		visit_all(node.program, *this);
		return &node;
    }

	PreNodeAST *visit(PreNodeMacroHeader &node) override {
		node.name->accept(*this);
		os << "(";
        node.args->accept(*this);
		os << ")";
		return &node;
    }

	PreNodeAST *visit(PreNodeMacroDefinition &node) override {
		os << "macro ";
        node.header->accept(*this);
		os << "\n";
        node.body->accept(*this);
		os << "end macro\n";
		return &node;
    }

	PreNodeAST *visit(PreNodeMacroCall &node) override {
        node.macro->accept(*this);
		return &node;
    }

	PreNodeAST *visit(PreNodeFunctionCall &node) override {
		node.function->accept(*this);
		return &node;
	}

	PreNodeAST *visit(PreNodeIterateMacro &node) override {
		os << "iterate_macro(";
        node.macro_call->accept(*this);
		os << ") on";
		node.iterator_start->accept(*this);
		os << " to ";
		node.iterator_end->accept(*this);
		if(node.step) {
			os << " step ";
		}
		node.step ->accept(*this);
		os << "\n";
		return &node;
    }

	PreNodeAST *visit(PreNodeLiterateMacro &node) override {
		os << "literate_macro(";
        node.macro_call->accept(*this);
		os << ") on ";
        node.literate_tokens->accept(*this);
		os << "\n";
		return &node;
    }

	PreNodeAST *visit(PreNodeIncrementer &node) override {
		os << "START_INC(";
        node.counter->accept(*this);
		os << ", ";
        node.iterator_start->accept(*this);
		os << ", ";
        node.iterator_step->accept(*this);
		os << ")\n";
		visit_all(node.body, *this);
		os << "END_INC\n";
		return &node;
    }

	void generate(const std::string& path) {
		std::ofstream out_file(path);
		if (out_file) {
			out_file << os.str();
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
	std::string get_indent() const {
		std::string result;
		for (int i = 0; i < m_scope_count; i++) {
			result += m_indent;
		}
		return result;
	}

};
