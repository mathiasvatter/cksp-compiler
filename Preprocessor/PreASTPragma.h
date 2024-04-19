//
// Created by Mathias Vatter on 24.01.24.
//

#pragma once

#include "PreASTVisitor.h"
#include "ImportProcessor.h"

class PreASTPragma : public PreASTVisitor {
public:
    [[nodiscard]] const std::string &get_output_path() const;

	void visit(PreNodePragma& node) override;
    void visit(PreNodeProgram &node) override;

private:

    std::set<std::string> m_pragma_directives = {"output_path"};
    std::string m_output_path;

};