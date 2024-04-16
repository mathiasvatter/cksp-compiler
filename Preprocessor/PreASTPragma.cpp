//
// Created by Mathias Vatter on 24.01.24.
//

#include "PreASTPragma.h"


void PreASTPragma::visit(PreNodeProgram& node) {
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

void PreASTPragma::visit(PreNodePragma& node) {
    auto option = node.option->get_string();
    auto token = node.option->keyword;
    auto it = m_pragma_directives.find(option);
    if(it == m_pragma_directives.end()) {
        CompileError(ErrorType::PreprocessorError, "Found unknown #pragma option.", token.line, "valid option.",option, token.file).exit();
    }
    auto argument = node.argument->keyword.val;
    std::string current_file = node.argument->keyword.file;
    if(node.argument->keyword.type == token::STRING) {
        // erase ""
        argument.erase(0,1);
        argument.pop_back();

        if(option == "output_path") {
            auto output_path = resolve_path(argument, token, current_file);
            if (output_path.is_error()) {
                output_path.get_error().exit();
            }
            if (std::filesystem::path(output_path.unwrap()).extension() == ".txt") {
                m_output_path = output_path.unwrap();
            }
        }
    }

}

const std::string &PreASTPragma::get_output_path() const {
    return m_output_path;
}
