//
// Created by Mathias Vatter on 23.09.23.
//

#pragma once

#include <utility>

#include "../misc/CommandLineOptions.h"
#include "../Processor/Processor.h"
#include "PreprocessorConditions.h"
#include "PreprocessorParser.h"
#include "PreAST/PreASTDesugar.h"
#include "PreAST/PreASTCombine.h"
#include "PreAST/PreASTIncrementer.h"
#include "PreAST/PreASTDefines.h"
#include "PreAST/PreASTPragma.h"

/// Bundles all preprocessor related classes and steps in one class
class Preprocessor {
	std::vector<Token> m_tokens{};

public:
    explicit Preprocessor(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

	~Preprocessor() = default;

	/// main function to process the tokens
    void process(CompilerConfig* config) {
    	auto result = Result<SuccessTag>(SuccessTag{});

    	PreprocessorConditions conditions(m_tokens);
    	result = conditions.process_conditions();
    	if(result.is_error()) {
    		auto error = result.get_error();
    		error.m_message += " Preprocessor failed while processing conditions.";
    		error.exit();
    	}
    	m_tokens = std::move(conditions.get_token_vector());

    	PreprocessorParser parser(m_tokens);
    	auto result_parse = parser.parse_program(nullptr);
    	if(result_parse.is_error()) {
    		auto error = result_parse.get_error();
    		error.m_message += " Preprocessor parsing failed.";
    		error.exit();
    	}
    	auto pre_ast = std::move(result_parse.unwrap());
    	PreASTPragma pragma(config);
    	pre_ast->accept(pragma);

    	PreASTDefines defines;
    	pre_ast->accept(defines);
    	pre_ast->debug_print();

    	PreASTDesugar desugar;
    	pre_ast->accept(desugar);
    	pre_ast->debug_print();

    	PreASTIncrementer incrementer;
    	pre_ast->accept(incrementer);
    	pre_ast->debug_print();

    	PreASTCombine combine;
    	pre_ast->accept(combine);

    	m_tokens = std::move(combine.m_tokens);
    }

	std::vector<Token> get_token_vector() {
		return std::move(m_tokens);
	}

};





