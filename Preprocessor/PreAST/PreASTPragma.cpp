//
// Created by Mathias Vatter on 24.01.24.
//

#include "PreASTPragma.h"
#include "../../misc/PathHandler.h"

void PreASTPragma::visit(PreNodeProgram& node) {
    m_program = &node;
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

void PreASTPragma::visit(PreNodePragma& node) {
    auto option = node.option->get_string();
    auto token = node.option->value;
    if(auto it = m_pragma_directives.find(option); it == m_pragma_directives.end()) {
        auto error = CompileError(ErrorType::PreprocessorError, "", "", token);
        error.m_message = "Found unknown <#pragma> option.";
        error.m_expected = "valid <#pragma> option.";
        error.m_got = option;
        error.exit();
    }
    auto argument = node.argument->value.val;
    std::string current_file = node.argument->value.file;
    if(node.argument->value.type == token::STRING) {
        // erase ""
        argument.erase(0,1);
        argument.pop_back();

        if(option == "output_path") {
			static PathHandler path_handler(token, current_file);
            auto output_path = path_handler.resolve_path(argument);
            if (output_path.is_error()) {
                output_path.get_error().exit();
            }
			auto valid_output_path = path_handler.generate_output_file(output_path.unwrap());
			if(valid_output_path.is_error()) {
				valid_output_path.get_error().exit();
			}
			m_program->output_path = valid_output_path.unwrap();
        }
    }

}
