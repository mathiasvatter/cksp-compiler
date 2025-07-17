//
// Created by Mathias Vatter on 15.07.25.
//

#pragma once

#include "ASTLowering.h"

/**
 * Transforms range nodes into initializer lists when applicable. (in assignment and declaration statements)
 * Will transform into loops if args cannot are not known at compile time.
 */
class LoweringRange final : public ASTLowering {
	DefinitionProvider* m_def_provider;
public:
	explicit LoweringRange(NodeProgram *program) : ASTLowering(program) {
		m_def_provider = program->def_provider;
	}

	NodeAST *visit(NodeRange &node) override {
		if (!node.start) {
			auto error = CompileError(ErrorType::SyntaxError, "<start> argument of <range> is not defined", "", node.tok);
			error.exit();
		}
		if (!node.step) {
			auto error = CompileError(ErrorType::SyntaxError, "<step> argument of <range> is not defined", "", node.tok);
			error.exit();
		}

		node.start->do_constant_folding();
		node.stop->do_constant_folding();
		node.step->do_constant_folding();

		if (node.parent->cast<NodeSingleAssignment>() or node.parent->cast<NodeSingleDeclaration>()) {
			if (!node.all_literals()) {
				auto error = CompileError(ErrorType::SyntaxError, "Range arguments must be literals in this context", "", node.tok);
				error.exit();
			} else {
				node.remove_references();
				return node.replace_with(node.to_initializer_list());
			}
		}

		return &node;
	}

};