//
// Created by Mathias Vatter on 24.01.24.
//

#include "PreprocessorPragma.h"
#include "PreprocessorImport.h"

PreprocessorPragma::PreprocessorPragma(const std::string &file)
	: Tokenizer(file) {}

void PreprocessorPragma::process() {
	if(m_input.empty())
		CompileError(ErrorType::TokenError, "Missing input file to tokenize.", 0, "<input file>", "", m_current_file).exit();

	while (m_pos < m_input_length - 1) {
		if(is_pragma()) {
			flush_buffer();
			// consume #pragma
			for(int i = 0; i<7; i++)
				consume();
			skip_whitespace();
			while(peek() != '\n') {
				flush_buffer();
				skip_whitespace();
				while(is_keyword_or_num() or peek() == '/' or peek() == '.') {
					consume();
				}
				m_pragma_options.push_back(m_buffer);
				consume();
			}
			break;
		} else {
			consume();
			flush_buffer();
		}
	}

	for (auto & opt : m_pragma_options) {
		if(std::filesystem::exists(opt)) {
			auto result = resolve_path(opt, Token(), m_input_file);
			if(result.is_error()) {
				result.get_error().exit();
			}
			m_output_path = result.unwrap();
		}
	}
}

bool PreprocessorPragma::is_pragma() {
	return peek(0) == '#' && peek(1) == 'p' && peek(2) == 'r' && peek(3) == 'a' && peek(4) == 'g' && peek(5) == 'm' && peek(6) == 'a';
}

const std::string &PreprocessorPragma::get_output_path() const {
	return m_output_path;
}


