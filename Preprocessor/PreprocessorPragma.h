//
// Created by Mathias Vatter on 24.01.24.
//

#pragma once

#include "../Tokenizer/Tokenizer.h"

class PreprocessorPragma : public Tokenizer {
public:
	explicit PreprocessorPragma(const std::string &file);
	~PreprocessorPragma() = default;
	void process();
	const std::string &get_output_path() const;
private:
	std::string m_input_file;
	bool is_pragma();
	std::vector<std::string> m_pragma_options;
	std::string m_output_path;
};
