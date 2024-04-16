//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreAST.h"


class PreASTVisitor {
public:
    virtual void visit(PreNodeNumber& node) {};
    virtual void visit(PreNodeInt& node) {};
    virtual void visit(PreNodeUnaryExpr& node) {
        node.operand->accept(*this);
    };
    virtual void visit(PreNodeBinaryExpr& node) {
        node.left->accept(*this);
        node.right->accept(*this);
    };
    virtual void visit(PreNodeKeyword& node) {};
    virtual void visit(PreNodeOther& node) {};
    virtual void visit(PreNodeDeadCode& node) {

	};
    virtual void visit(PreNodePragma& node) {
//        node.option->accept(*this);
//        node.argument->accept(*this);
    };
    virtual void visit(PreNodeStatement& node) {
        node.statement->accept(*this);
    };
    virtual void visit(PreNodeChunk& node) {
        for(auto & n : node.chunk) {
            n->accept(*this);
        }
    };
    virtual void visit(PreNodeDefineHeader& node) {
        node.args->accept(*this);
    };
    virtual void visit(PreNodeList& node) {
        for(auto & param : node.params) {
            param->accept(*this);
        }
    };
    virtual void visit(PreNodeDefineStatement& node) {
        node.header->accept(*this);
        node.body->accept(*this);
    };
    virtual void visit(PreNodeDefineCall& node) {
        node.define->accept(*this);
    };
    virtual void visit(PreNodeProgram& node) {
        for(auto & def : node.define_statements) {
            def->accept(*this);
        }
        for(auto & n : node.program) {
            n->accept(*this);
        }
    };
    virtual void visit(PreNodeMacroHeader& node) {
		node.name->accept(*this);
        node.args->accept(*this);
    };
    virtual void visit(PreNodeMacroDefinition& node) {
        node.header->accept(*this);
        node.body->accept(*this);
    };
    virtual void visit(PreNodeMacroCall& node) {
        node.macro->accept(*this);
    };
	virtual void visit(PreNodeFunctionCall& node) {
		node.function->accept(*this);
	};
    virtual void visit(PreNodeIterateMacro& node) {
		node.iterator_start->accept(*this);
		node.iterator_end->accept(*this);
		node.step ->accept(*this);
        node.macro_call->accept(*this);
    };
    virtual void visit(PreNodeLiterateMacro& node) {
        node.literate_tokens->accept(*this);
        node.macro_call->accept(*this);
    };
    virtual void visit(PreNodeIncrementer& node) {
        node.counter->accept(*this);
        node.iterator_start->accept(*this);
        node.iterator_step->accept(*this);
        for(auto &b : node.body) {
            b->accept(*this);
        }
    };

protected:
	/// puts nested statement list in one, returns new vector to replace node->statements with
	inline static std::vector<std::unique_ptr<PreNodeAST>> cleanup_node_chunk(PreNodeChunk* node) {
		std::vector<std::unique_ptr<PreNodeAST>> temp;
		for(auto & i : node->chunk) {
			if(auto node_statement = safe_cast<PreNodeStatement>(i.get(), PreNodeType::STATEMENT)) {
				if(auto node_chunk = safe_cast<PreNodeChunk>(node_statement->statement.get(), PreNodeType::CHUNK)) {
					// Fügen Sie die inneren Statements zum temporären Vector hinzu
					auto &inner_chunk = node_chunk->chunk;
					temp.insert(temp.end(),
								std::make_move_iterator(inner_chunk.begin()),
								std::make_move_iterator(inner_chunk.end())
					);
					// Markieren Sie das aktuelle Element zur Löschung
					i = nullptr;
					continue; // Überspringen Sie das erhöhen des Indexes
				}
			}
			// Fügen Sie das aktuelle Element zum temporären Vector hinzu, wenn es nicht gelöscht werden soll
			temp.push_back(std::move(i));
		}
		// Ersetzen Sie die alte Liste durch die neue
		return std::move(temp);
	}
};
