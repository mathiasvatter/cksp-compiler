//
// Created by Mathias Vatter on 24.01.24.
//

#pragma once

#include "PreASTVisitor.h"
#include "../ImportProcessor.h"

class PreASTPragma final : public PreASTVisitor {
public:
	explicit PreASTPragma(PreNodeProgram* program) : PreASTVisitor(program) {}
	void visit(PreNodePragma& node) override;
    void visit(PreNodeProgram &node) override;

private:
    std::set<std::string> m_pragma_directives = {"output_path"};

};