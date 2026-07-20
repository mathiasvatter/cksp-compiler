//
// Created by Mathias Vatter on 18.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../SyntaxChecks/KSPPersistency.h"
#include "../SyntaxChecks/KSPDeclarations.h"
#include "../Optimization/MemoryExhaustedNesting.h"
#include "../Lowering/LoweringFunctionCall.h"
#include "../SyntaxChecks/KSPConditions.h"
#include "FunctionHandling/BuiltinRestrictionValidator.h"


/**
 * Checks the AST for nodes that should not exist anymore or empty types.
 */
class ASTCheck final : public ASTVisitor {
	DefinitionProvider *m_def_provider;

public:
	explicit ASTCheck(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if (TypeRegistry::get_identifier_from_type(node.variable->ty) == ' ') {
			auto error = ASTVisitor::make_diagnostic(ErrorType::InternalError, node);
			error.set_message("Type identifier for variable '" + node.variable->name + "' is empty. This should not happen.");
			error.exit();
		}
		if(node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeWildcard& node) override {
		auto error = ASTVisitor::make_diagnostic(ErrorType::InternalError, node);
		error.set_message("<wildcard> node should not exist anymore in AST.");
		error.exit();
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeArray& node) override {
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariableRef& node) override {
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeGetControl& node) override {
		auto error = ASTVisitor::make_diagnostic(ErrorType::InternalError, node);
		error.set_message("<NodeGetControl> node should not exist anymore in AST.");
		error.exit();
		return &node;
	}

	NodeAST* visit(NodeSetControl& node) override {
		auto error = ASTVisitor::make_diagnostic(ErrorType::InternalError, node);
		error.set_message("<NodeSetControl> node should not exist anymore in AST.");
		error.exit();
		return &node;
	}

};