//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDesugar final : public PreASTVisitor {
public:
	// transform to macro calls if macro definition exists, otherwise return node
	PreNodeAST *visit(PreNodeFunctionCall &node) override;

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

	// std::unordered_map<StringIntKey, PreNodeMacroDefinition*, StringIntKeyHash> m_macro_lookup;

	// macro lookup without args because iterate and literate macro calls do not have args in their constructs
    std::unordered_map<std::string, PreNodeMacroDefinition*> m_macro_string_lookup;
	// only uses macro name without args for lookup
	PreNodeMacroDefinition* get_macro_string_definition(const PreNodeMacroHeader& macro_header);

	PreNodeAST *do_substitution(PreNodeLiteral &node);
    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
    static std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_map(PreNodeMacroHeader& definition, const PreNodeMacroHeader& call);
    // PreNodeMacroDefinition* get_macro_definition(const PreNodeMacroHeader& macro_header);



    void check_recursion(const Token &tok) const;
    std::unordered_set<std::string> m_macros_used;
};



