//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once

#include <unordered_map>

#include "Tokenizer/Tokenizer.h"
#include "AST/AST.h"
#include "Tokenizer/Tokens.h"
#include "Result.h"


// Hilfsfunktion, die das Result-Objekt zurückgibt, wenn kein Fehler vorliegt.
template<typename T> Result<T> handle_error(Result<T> result) {
	if (result.is_error()) {
		return Result<T>(result.get_error());
	}
	return result; // Return the success result directly
}

inline static std::map<token, int> BinaryOpPrecendence = {
        {token::BOOL_OR, 1},
        {token::BOOL_AND, 2},
		{token::BOOL_NOT, 3},
//        {token::GREATER_THAN, 4},
//        {token::LESS_THAN, 5},
//        {token::GREATER_EQUAL, 6},
//        {token::LESS_EQUAL, 7},
//		{token::EQUAL, 7},
		{token::COMPARISON, 7},
        {token::BIT_XOR, 8},
        {token::BIT_OR, 9},
        {token::BIT_AND, 10},
        {token::BIT_NOT, 11},
        {token::ADD, 12},
        {token::SUB, 12},
        {token::MULT, 13},
        {token::DIV, 13},
        {token::MODULO, 13}
};

inline static bool is_unary_operator(token op) {
    return op == token::SUB || op == token::BIT_NOT || op == token::BOOL_NOT;
}

inline static int _get_binop_precedence(token tok) {
	int precedence = BinaryOpPrecendence[tok];
	if (precedence <= 0) {
		return -1;
	}
	return precedence;
}


class Parser {

public:
    explicit Parser(std::vector<Token> tokens);
    [[nodiscard]] size_t get_current_pos() const;
    void set_current_pos(size_t mPos);

    Result<std::unique_ptr<NodeProgram>> parse();

protected:
	size_t m_pos;
    std::vector<Token> m_tokens;
	token m_curr_token;
    std::string m_curr_token_value;

	[[nodiscard]] Token peek(int ahead = 0);
	Token consume();
    Token& get_tok();
	void _skip_linebreaks();

    static std::optional<Token> get_persistent_keyword(const Token& tok);
    static std::string sanitize_binary(const std::string& input);
    /// convert eg 0bFFFh into 0xbFFF
    static std::string sanitize_hex(const std::string& input);
    Result<std::unique_ptr<NodeInt>> parse_int(const Token& tok, int base, NodeAST* parent);

    Result<std::unique_ptr<NodeAST>> parse_number(NodeAST* parent);
    Result<std::unique_ptr<NodeString>> parse_string(NodeAST* parent);
    Result<std::unique_ptr<NodeVariable>> parse_variable(NodeAST* parent, std::optional<Token> is_persistent=std::optional<Token>(), VarType var_type=VarType::Mutable);
    Result<std::unique_ptr<NodeArray>> parse_array(NodeAST* parent, std::optional<Token> is_persistent=std::optional<Token>(), VarType var_type=VarType::Array);
    Result<std::unique_ptr<NodeParamList>> parse_param_list(NodeAST* parent);
	Result<SuccessTag> _parse_into_param_list(std::vector<std::unique_ptr<NodeAST>>& params, NodeAST* parent);
    /// parses every expression from binary, string, unary to number and variable
    Result<std::unique_ptr<NodeAST>> parse_expression(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_string_expr(NodeAST* parent);
        /// Helper function for parsing binary string expression recursively
        Result<std::unique_ptr<NodeAST>> _parse_string_expr_rhs(std::unique_ptr<NodeAST> lhs, NodeAST* parent);
    /// parse unary or binary expression
    Result<std::unique_ptr<NodeAST>> parse_binary_expr(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_unary_expr(NodeAST* parent);
	    /// Helper function for parsing binary expressions recursion
		Result<std::unique_ptr<NodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs, NodeAST* parent);
		/// ( expression )
		Result<std::unique_ptr<NodeAST>> _parse_parenth_expr(NodeAST* parent);
		/// parse identifierexpr, numberexpr, parenthexpr, functionheader
		Result<std::unique_ptr<NodeAST>> _parse_primary_expr(NodeAST* parent);
    Result<std::unique_ptr<NodeAssignStatement>> parse_assign_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeSingleAssignStatement>> parse_single_assign_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeAssignStatement>> parse_into_assign_statement(std::unique_ptr<NodeParamList> array_variable, NodeAST* parent);
    Result<std::unique_ptr<NodeVariable>> parse_declare_variable(NodeAST* parent);
    Result<std::unique_ptr<NodeArray>> parse_declare_array(NodeAST* parent);
    Result<std::unique_ptr<NodeUIControl>> parse_declare_ui_control(NodeAST* parent);
    Result<std::unique_ptr<NodeDeclareStatement>> parse_declare_statement(NodeAST* parent);
//	Result<std::unique_ptr<NodeDefineStatement>> parse_define_statement();
    Result<std::unique_ptr<NodeAST>> parse_const_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_list_block(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_family_statement(NodeAST* parent);

	/// combines all possible statement types
    Result<std::unique_ptr<NodeStatement>> parse_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeIfStatement>> parse_if_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeForStatement>> parse_for_statement(NodeAST* parent);
    bool is_ranged_for_loop();
    Result<std::unique_ptr<NodeRangedForStatement>> parse_ranged_for_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeWhileStatement>> parse_while_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeSelectStatement>> parse_select_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeGetControlStatement>> parse_get_control_statement(std::unique_ptr<NodeAST> ui_id, NodeAST* parent);
    Result<std::unique_ptr<NodeFunctionDefinition>> parse_function_definition(NodeAST* parent);
    Result<std::unique_ptr<NodeFunctionHeader>> parse_function_header(NodeAST* parent);
    Result<std::unique_ptr<NodeFunctionCall>> parse_function_call(NodeAST* parent);
    Result<std::unique_ptr<NodeCallback>> parse_callback(NodeAST* parent);

	Result<std::unique_ptr<NodeProgram>> parse_program();
    std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_function_definitions;
//    std::unordered_map<std::string, NodeFunctionDefinition*> m_functions;
    void mark_function_as_used(const std::string& func_name, int num_args);

	Result<SuccessTag> consume_linebreak(const std::string& construct);
private:
    bool is_variable_declaration();
    bool is_array_declaration();

};

