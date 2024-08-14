//
// Created by Mathias Vatter on 23.09.23.
//

#pragma once
#include <unordered_set>
#include <utility>

#include "../AST/ASTNodes/AST.h"
#include "../Processor/Processor.h"

/// Bundles all preprocessor related classes and steps in one class
class Preprocessor {
public:
    explicit Preprocessor(std::vector<Token> tokens);
	~Preprocessor() = default;

	/// main function to process the tokens
    void process();
    [[nodiscard]] const std::string &get_output_path() const;
	std::vector<Token> get_token_vector();

private:
	std::vector<Token> m_tokens{};
	/// output path from pragma
    std::string m_output_path;

};





