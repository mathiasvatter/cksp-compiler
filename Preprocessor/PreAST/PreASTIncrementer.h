//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTIncrementer final : public PreASTVisitor {
public:
	explicit PreASTIncrementer(PreNodeProgram* program) : PreASTVisitor(program) {}
    void visit(PreNodeProgram& node) override;
    void visit(PreNodeNumber& node) override;
    void visit(PreNodeInt& node) override;
    void visit(PreNodeKeyword& node) override;
    void visit(PreNodeIncrementer& node) override;
	/// move to macro_defitions vector
	void visit(PreNodeMacroDefinition& node) override;


private:
	PreNodeProgram* m_program = nullptr;

	bool substitute_with_incremented_value(const std::string& name, size_t line, PreNodeAST* node);

	/// counter_var, step_value, PreAST node_int
    std::vector<std::tuple<std::string, int32_t, std::unique_ptr<PreNodeInt>>> m_incrementer_stack;
	/// node to replace, line
    std::vector<std::pair<std::unique_ptr<PreNodeAST>, size_t>> m_last_incrementer_var;
	PreNodeInt* get_substitute(const std::string& name);
	/// returns step value of counter_var in m_incrementer_stack -> will return 0 if not found
	int32_t get_step_value(const std::string& name);
    bool update_last_incrementer_var(const PreNodeAST* node, PreNodeInt* substitute, const std::string& node_val, size_t node_line);
};
