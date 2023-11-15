//
// Created by Mathias Vatter on 13.11.23.
//

#pragma once

#include "PreAST.h"

class SimpleExprInterpreter {
public:
    explicit SimpleExprInterpreter(std::unique_ptr<PreNodeChunk> chunk);
    Result<int> evaluate_int_expression(std::unique_ptr<PreNodeAST>& root);

	std::vector<std::unique_ptr<PreNodeAST>> m_nodes;
	size_t m_pos = 0;
	int m_line;
	std::string m_file;
	[[nodiscard]] PreNodeAST* peek(int ahead = 0);
	std::unique_ptr<PreNodeAST> consume();

	Result<std::unique_ptr<PreNodeAST>> parse();
	Result<std::unique_ptr<PreNodeAST>> parse_binary_expr(PreNodeAST *parent);
	Result<std::unique_ptr<PreNodeAST>> _parse_primary_expr(PreNodeAST *parent);
	Result<std::unique_ptr<PreNodeAST>> parse_unary_expr(PreNodeAST *parent);
	Result<std::unique_ptr<PreNodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<PreNodeAST> lhs, PreNodeAST *parent);
	/// ( expression )
	Result<std::unique_ptr<PreNodeAST>> _parse_parenth_expr(PreNodeAST *parent);
};


