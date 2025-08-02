//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDesugar final : public PreASTVisitor {
public:
	PreNodeAST *visit(PreNodeProgram &node) override;
    PreNodeAST *visit(PreNodeNumber &node) override;
    PreNodeAST *visit(PreNodeInt &node) override;
    PreNodeAST *visit(PreNodeKeyword &node) override;
    PreNodeAST *visit(PreNodeOther &node) override;
    PreNodeAST *visit(PreNodeStatement &node) override;
    PreNodeAST *visit(PreNodeChunk &node) override;
    PreNodeAST *visit(PreNodeList &node) override;
	PreNodeAST *visit(PreNodeMacroCall &node) override;
	PreNodeAST *visit(PreNodeMacroHeader &node) override;
	PreNodeAST *visit(PreNodeIterateMacro &node) override;
	PreNodeAST *visit(PreNodeLiterateMacro &node) override;
	PreNodeAST *visit(PreNodeIncrementer &node) override;


private:
	std::string m_debug_token;

	std::unordered_map<StringIntKey, PreNodeMacroDefinition*, StringIntKeyHash> m_macro_lookup;
    std::unordered_map<std::string, PreNodeMacroDefinition*> m_macro_string_lookup;

	PreNodeAST *do_substitution(PreNodeLiteral &node);
    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
    static std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_map(PreNodeMacroHeader& definition, const PreNodeMacroHeader& call);
    PreNodeMacroDefinition* get_macro_definition(const PreNodeMacroHeader& macro_header);
    std::string get_text_replacement(const Token& name);

	std::stack<std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>>> m_substitution_stack;

    void check_recursion(const Token &tok) const;
    std::unordered_set<std::string> m_macros_used;
};



