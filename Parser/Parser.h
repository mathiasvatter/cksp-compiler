//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once

// #include <unordered_map>

#include "../AST/ASTNodes/AST.h"
#include "../AST/ASTNodes/ASTDataStructures.h"
#include "../misc/Result.h"
#include "../Processor/Processor.h"
#include "../AST/ASTNodes/ASTReferences.h"

// Hilfsfunktion, die das Result-Objekt zurückgibt, wenn kein Fehler vorliegt.
template<typename T> Result<T> handle_error(Result<T> result) {
	if (result.is_error()) {
		return Result<T>(result.get_error());
	}
	return result; // Return the success result directly
}

inline static std::map<token, int> operator_precedence = {
        {token::BOOL_XOR, 1},
        {token::BOOL_OR, 1},
        {token::BOOL_AND, 2},
		{token::BOOL_NOT, 3},
        {token::GREATER_THAN, 7},
        {token::LESS_THAN, 7},
        {token::GREATER_EQUAL, 7},
        {token::LESS_EQUAL, 7},
		{token::EQUAL, 7},
		{token::NOT_EQUAL, 7},
        {token::BIT_XOR, 8},
        {token::BIT_OR, 9},
        {token::BIT_AND, 10},
		{token::SHIFT_RIGHT, 11},
		{token::SHIFT_LEFT, 11},
        {token::BIT_NOT, 12},
        {token::ADD, 13},
        {token::SUB, 13},
        {token::MULT, 14},
        {token::DIV, 14},
        {token::MODULO, 14},
		{token::EXP,15}
};

static const std::unordered_set<token> modifier_keywords = {
	token::READ, token::PERS, token::INSTPERS, token::CONST,
	token::POLYPHONIC, token::LOCAL, token::GLOBAL, token::STATIC, token::UI_CONTROL
};
static const std::unordered_set<token> persistence_keywords = {
	token::READ, token::PERS, token::INSTPERS
};

static int _get_binop_precedence(const token tok) {
	const int precedence = operator_precedence[tok];
	if (precedence <= 0) {
		return -1;
	}
	return precedence;
}


class Parser: public Processor {

public:
    explicit Parser(std::vector<Token> tokens);
    Result<std::unique_ptr<NodeProgram>> parse();

    static std::optional<Token> get_persistent_keyword(const Token& tok);
	int peek_past_modifiers();

    static std::string sanitize_binary(const std::string& input);
    /// convert eg 0bFFFh into 0xbFFF
    static std::string sanitize_hex(const std::string& input);

	Result<std::unique_ptr<NodeAST>> parse_wildcard(NodeAST* parent);
    static Result<std::unique_ptr<NodeInt>> parse_int(const Token& tok, int base, NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_number(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_nil(NodeAST* parent);
    Result<std::unique_ptr<NodeString>> parse_string(NodeAST* parent);
    Result<std::unique_ptr<NodeVariable>> parse_variable(NodeAST* parent, const std::optional<Token>& is_persistent=std::optional<Token>(), DataType var_type=DataType::Mutable);
	Result<std::unique_ptr<NodeVariableRef>> parse_variable_ref(NodeAST* parent);
	Result<std::unique_ptr<NodePointer>> parse_pointer(NodeAST* parent, const std::optional<Token>& is_persistent=std::optional<Token>());
	Result<std::unique_ptr<NodePointerRef>> parse_pointer_ref(NodeAST* parent);
    Result<std::unique_ptr<NodeDataStructure>> parse_array(NodeAST *parent, std::optional<Token> is_persistent = std::optional<Token>(), DataType var_type = DataType::Mutable);
	Result<std::unique_ptr<NodeReference>> parse_array_ref(NodeAST *parent);
	Result<std::unique_ptr<NodeAST>> parse_reference_chain(NodeAST *parent);

	Result<std::unique_ptr<NodeParamList>> parse_multiple_values(NodeAST* parent);
    Result<std::unique_ptr<NodeParamList>> parse_param_list(NodeAST* parent);
	Result<SuccessTag> _parse_into_param_list(std::vector<std::unique_ptr<NodeAST>>& params, NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_init_list(NodeAST* parent);
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
    Result<std::unique_ptr<NodeDeclaration>> parse_declare_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeAssignment>> parse_assign_statement(std::vector<std::unique_ptr<NodeReference>> l_values, NodeAST* parent);
	Result<std::unique_ptr<NodeCompoundAssignment>> parse_compound_assign_statement(std::unique_ptr<NodeReference> l_value, NodeAST* parent);
	Result<std::vector<std::unique_ptr<NodeReference>>> parse_l_values(NodeAST* parent);
    static bool is_func_call_reference_chain(NodeReference& ref);
	Result<std::unique_ptr<NodeReturn>> parse_return_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeDelete>> parse_delete_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeBreak>> parse_break_statement(NodeAST* parent);

    Result<std::unique_ptr<NodeSingleAssignment>> parse_single_assign_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeSingleDeclaration>> parse_single_declare_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeFunctionParam>> parse_function_param(NodeAST* parent);
    Result<std::unique_ptr<NodeVariable>> parse_declare_variable(NodeAST* parent);
    Result<std::unique_ptr<NodeDataStructure>> parse_declare_array(NodeAST* parent);
    Result<std::unique_ptr<NodeUIControl>> parse_declare_ui_control(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_const_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_list_block(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_family_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeStruct>> parse_struct(NodeAST* parent);

	/// combines all possible statement types
    Result<std::unique_ptr<NodeStatement>> parse_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeIf>> parse_if_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeFor>> parse_for_statement(NodeAST* parent);
    bool is_for_each_syntax();
    Result<std::unique_ptr<NodeForEach>> parse_for_each_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeWhile>> parse_while_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeSelect>> parse_select_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeGetControl>> parse_get_control_statement(std::unique_ptr<NodeAST> ui_id, NodeAST* parent);
    Result<std::shared_ptr<NodeFunctionDefinition>> parse_function_definition(NodeAST* parent);
	std::shared_ptr<NodeFunctionDefinition> m_current_function_def;
    /// function params are no references -> replace with references
    Result<std::unique_ptr<NodeParamList>> parse_function_args(NodeAST* parent);
	Result<std::unique_ptr<NodeFunctionHeader>> parse_function_header(NodeAST* parent);
	Result<std::unique_ptr<NodeFunctionHeaderRef>> parse_function_header_ref(NodeAST* parent);
    Result<std::unique_ptr<NodeFunctionCall>> parse_function_call(NodeAST* parent);
    Result<std::unique_ptr<NodeCallback>> parse_callback(NodeAST* parent);
	Result<std::unique_ptr<NodeNamespace>> parse_namespace(NodeAST* parent);

	Result<std::unique_ptr<NodeProgram>> parse_program();
	std::vector<std::unique_ptr<NodeCallback>> m_callbacks;
	int m_init_callback_idx = -1;
    std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_function_definitions;
	std::vector<NodeDataStructure*> m_all_data_structures;

	Result<SuccessTag> consume_linebreak(const std::string& construct);

private:
    bool is_variable_declaration();
    bool is_array_declaration();
};

