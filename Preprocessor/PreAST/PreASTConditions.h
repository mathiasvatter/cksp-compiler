//
// Created by Mathias Vatter on 25.02.26.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTConditions final : public PreASTVisitor {
	std::unordered_map<std::string, bool> m_conditions_set{};

	bool set_condition(const std::unique_ptr<PreNodeKeyword> &condition, const bool value) {
		const auto name = condition->tok.val;
		m_conditions_set[name] = value;
		return value;
	}

public:
	explicit PreASTConditions() {
		m_conditions_set.clear();
	}

	PreNodeAST *visit(PreNodeSetCondition &node) override {
		set_condition(node.condition, true);
		return &node;
	}

	PreNodeAST *visit(PreNodeResetCondition &node) override {
		set_condition(node.condition, false);
		return &node;
	}

	PreNodeAST *visit(PreNodeUseCodeIf &node) override {
		const auto condition_name = node.condition->tok.val;
		const auto it = m_conditions_set.find(condition_name);
		if(it == m_conditions_set.end()) {
			auto error = CompileError(ErrorType::PreprocessorError, "", "", node.tok);
			error.m_message = "Condition '" + condition_name + "' has not been defined. <USE_CODE_IF> and <USE_CODE_IF_NOT> "
						"statements require a condition to be defined once with SET_CONDITION(<condition>) or RESET_CONDITION(<condition>) before usage.";
			error.exit();
		}

		bool condition_value = it->second;
		if (condition_value and node.if_branch) {
			node.if_branch->accept(*this);
			return node.replace_with(std::move(node.if_branch));
		}
		if (!condition_value and node.else_branch) {
			node.else_branch->accept(*this);
			return node.replace_with(std::move(node.else_branch));
		}

		return node.replace_with(std::make_unique<PreNodeDeadCode>(node.tok, node.parent));

	}

	PreNodeAST *visit(PreNodeProgram &node) override {
		m_program = &node;
		visit_all(node.define_statements, *this);
		visit_all(node.macro_definitions, *this);
		node.program->accept(*this);
		return &node;
	}

	PreNodeAST* visit(PreNodeChunk &node) override {
		visit_all(node.chunk, *this);
		node.flatten();
		return &node;
	}


};
