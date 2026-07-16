//
// Created by Mathias Vatter on 27.04.26.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * This class checks a function that has exit commands in its body by doing the following:
 * -> see issue #98
 *  - CompileWarning: alerting the user that using exit in a function body will ALWAYS only exit the function it is
 *    called from, never the callback that calls the function
 *  - it will suggest to only use return statements when exiting functions prematurely
 *  - CompileError: when encountering return AND exit statements
 *  - add extra exit statement at the very end of the function body so that the transformation in return stmts
 *    later on does not lead to infinite loops as encountered in issue #98
 */
class FunctionExitCheck final : public ASTVisitor {
public:

	NodeAST* sanitize_exit(NodeFunctionDefinition& def) {
		// no need for validation if function has_exit_command is tagged as false
		if (!def.has_exit_command) {
			return &def;
		}
		def.accept(*this);

		auto warning = CompileError(ErrorType::CompileWarning, "", "", def.tok);
		warning.set_message("Note: Using <exit> in a function body will only exit the function it is called from, never the callback that calls the function.\n"
					  "Please use <return> statements to exit functions prematurely instead of <exit> statements. This will make the behavior of your code clearer and prevent confusion about the behavior of <exit> statements in functions.");
		warning.print();

		return &def;
	}

private:

	NodeAST* visit(NodeBlock& node) override {
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionDefinition &node) override {
		ASTVisitor::visit(node);
		// node.body->accept(*this);
		if (auto exit = node.body->get_last_statement()->cast<NodeFunctionCall>()) {
			if (exit->is_builtin_kind() and exit->function->name == "exit") {
				return &node;
			}
		}
		// add extra exit statement at the very end of the function body so that the transformation in return
		// stmts later on does not lead to infinite loops as encountered in issue #98
		auto new_exit = DefinitionProvider::create_builtin_call("exit");
		new_exit->ty = TypeRegistry::Void;
		node.body->add_as_stmt(std::move(new_exit));
		return &node;
	}

	NodeAST* visit(NodeReturn &node) override {
		auto error = CompileError(ErrorType::CompileError, "", "", node.tok);
		error.set_message("A function that contains <exit> statements cannot contain <return>. Please use either <return> or <exit>.\nI suggest"
		" using <return> only since it states the desired behavior to prematurely return from the function more clearly and is in-line with the syntax in other programming languages.");
		error.exit();
		return &node;
	}



};