//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"

/// complete ASTNodes like arrays, ui controls, etc., fill in declaration information by
/// tracking data structure definitions with DefinitionProvider
class ASTBuildDataStructures: public ASTVisitor {
public:
	explicit ASTBuildDataStructures(DefinitionProvider* definition_provider);
	void visit(NodeBody& node) override;
	void visit(NodeUIControl& node) override;
	void visit(NodeArray& node) override;
	void visit(NodeNDArray& node) override;
	void visit(NodeVariable& node) override;

private:
	DefinitionProvider* m_def_provider;

    static inline ASTType infer_type_from_identifier(std::string& var_name) {
        ASTType type = ASTType::Unknown;
        if(contains(VAR_IDENT, var_name[0]) || contains(ARRAY_IDENT, var_name[0])) {
            std::string identifier(1, var_name[0]);
            var_name = var_name.erase(0,1);
            token token_type = *get_token_type(TYPES, identifier);
            type = token_to_type(token_type);
        }
        return type;
    }
};

