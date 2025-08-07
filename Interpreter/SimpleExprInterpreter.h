//
// Created by Mathias Vatter on 13.11.23.
//

#pragma once

#include "../Preprocessor/PreAST/PreASTNodes/PreAST.h"

class SimpleExprInterpreter {
public:
    explicit SimpleExprInterpreter(const std::string& file, const int line) : m_line(line), m_file(file) {}
	explicit SimpleExprInterpreter(const Token& tok) : m_line(tok.line), m_file(tok.file) {}
    Result<int> parse_and_evaluate(std::vector<std::unique_ptr<PreNodeAST>> n);

//	std::vector<std::unique_ptr<PreNodeAST>> m_nodes;
	size_t m_pos = 0;
	int m_line;
	std::string m_file;
	[[nodiscard]] PreNodeAST* peek(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, int ahead = 0) const;
	std::unique_ptr<PreNodeAST> consume(const std::vector<std::unique_ptr<PreNodeAST>>& nodes);

    Result<int> evaluate_int_expression(std::unique_ptr<PreNodeAST>& root);
	Result<std::unique_ptr<PreNodeAST>> parse(std::vector<std::unique_ptr<PreNodeAST>> n);
	Result<std::unique_ptr<PreNodeAST>> parse_binary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent);
	Result<std::unique_ptr<PreNodeAST>> _parse_primary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent);
	Result<std::unique_ptr<PreNodeAST>> parse_unary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent);
	Result<std::unique_ptr<PreNodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<PreNodeAST> lhs, const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent);
	/// ( expression )
	Result<std::unique_ptr<PreNodeAST>> _parse_parenth_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent);
};


class SimpleInterpreter {
public:
    explicit SimpleInterpreter() = default;
    Result<int> evaluate_int_expression(std::unique_ptr<NodeAST>& root);
};


