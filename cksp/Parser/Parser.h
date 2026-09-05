//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once

// #include <unordered_map>

#include "../ASTNodes/AST.h"
#include "../ASTNodes/ASTDataStructures.h"
#include "../../misc/Result.h"
#include "../Processor/Processor.h"
#include "../ASTNodes/ASTReferences.h"
#include "../Migration/PropertyMigration.h"
#include "../Migration/ReservedResultMigration.h"
#include "../Migration/TaskfuncMigration.h"

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
		{token::SHIFT_RIGHT_LOGICAL, 11},
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



class Parser: public Processor {
	NodeProgram* m_program = nullptr;

	std::vector<std::unique_ptr<NodeCallback>> m_callbacks;
	int m_init_callback_idx = -1;
    std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_function_definitions;
	std::vector<NodeDataStructure*> m_all_data_structures;

	bool is_variable_declaration();
	bool is_array_declaration();
	static constexpr int NO_TYPE_ARGUMENTS = -1;
	/// Index of the token following a type argument list written directly after the name at
	/// <peek()>, or <NO_TYPE_ARGUMENTS> when what follows the name is not one. The shape alone
	/// decides, so a comparison such as <a < b> - which never reaches a closing <>> - is not
	/// mistaken for a type argument list.
	[[nodiscard]] int type_argument_list_end() const;
	/// True for the token shape <Name<T, U>()>. Comparisons such as <a < b> stay expressions.
	[[nodiscard]] bool looks_like_parameterized_call() const;
	/// True for the token shape <Name<T, U>.member>: an access chain qualified by a parameterized
	/// type rather than by an instance.
	[[nodiscard]] bool looks_like_type_qualified_access() const;
	/// <property name> alone on its line: the head of a SublimeKSP property block.
	///
	/// Recognised by shape rather than by a reserved keyword. <property> is an ordinary word -
	/// two shipped builtins take a parameter of that name, and parameter names reach the user
	/// through completion signatures - so reserving it would break them and every script that
	/// declares a variable called <property>. Two bare keywords followed by a linebreak is not
	/// a CKSP statement in any position, which makes the shape unambiguous on its own.
	bool is_sublime_property();
	static bool is_malformed_end_statement_start(const Token& tok, const Token& next);
	static Diagnostic make_invalid_end_statement_diagnostic(const std::string& construct, const std::string& expected, const Token& start, const Token& next);
	static Type* normalize_ksp_identifier_token(Token& token);

public:

    explicit Parser(std::vector<Token> tokens);
    Result<std::unique_ptr<NodeProgram>> parse();

    static std::optional<Token> get_persistent_keyword(const Token& tok);
	int peek_past_modifiers();
	static Diagnostic make_declare_modifier_diagnostic(const Token& found);
	/// The diagnostic for a place that needs a name and found one of CKSP's own words.
	///
	/// "expected: keyword, got: xor" leaves the reader to work out that <xor> is spelled like
	/// a name but is the boolean operator. Naming what the word is reserved for is the whole
	/// answer, and it is the answer a ported SublimeKSP script needs most: that dialect
	/// reserves fewer words, so <function xor(a, b)> is ordinary there.
	static Diagnostic make_name_expected_diagnostic(
		ErrorType type, std::string message, std::string expected, const Token& found);
	static std::optional<Diagnostic> check_invalid_end_statement(const std::string& construct, token expected_end, const Token& start, const Token& next);

	static int get_binop_precedence(const token tok) {
		const int precedence = operator_precedence[tok];
		if (precedence <= 0) {
			return -1;
		}
		return precedence;
	}

    static std::string sanitize_binary(const std::string& input);
    /// convert eg 0bFFFh into 0xbFFF
    static std::string sanitize_hex(const std::string& input);

	Result<std::unique_ptr<NodeAST>> parse_wildcard(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_member_path(NodeAST* parent);
	static Result<std::unique_ptr<NodeInt>> parse_int(const Token& tok, int base, NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_number(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_nil(NodeAST* parent);
    Result<std::unique_ptr<NodeString>> parse_string(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_boolean(NodeAST* parent);
	Result<std::unique_ptr<NodeFormatString>> parse_fstring(NodeAST* parent);
    Result<std::unique_ptr<NodeVariable>> parse_variable(NodeAST* parent, const std::optional<Token>& is_persistent=std::optional<Token>(), DataType var_type=DataType::Mutable);
	Result<std::unique_ptr<NodeVariableRef>> parse_variable_ref(NodeAST* parent);
	/// <List<int>.MAX>: the leading element of an access chain qualified by a parameterized type.
	/// The reference carries the <ObjectType> instead of a name for the resolver to look up, and
	/// registering that type also queues the instantiation - the struct therefore exists even when
	/// nothing else in the script constructs one.
	Result<std::unique_ptr<NodeVariableRef>> parse_type_qualifier(NodeAST* parent);
	Result<std::unique_ptr<NodePointer>> parse_pointer(NodeAST* parent, const std::optional<Token>& is_persistent=std::optional<Token>());
	Result<std::unique_ptr<NodePointerRef>> parse_pointer_ref(NodeAST* parent);
    Result<std::unique_ptr<NodeDataStructure>> parse_array(NodeAST *parent, std::optional<Token> is_persistent = std::optional<Token>(), DataType var_type = DataType::Mutable);
	Result<std::unique_ptr<NodeReference>> parse_array_ref(NodeAST *parent);
	Result<std::unique_ptr<NodeAST>> parse_reference_chain(NodeAST *parent);

	Result<std::unique_ptr<NodeParamList>> parse_multiple_values(NodeAST* parent);
    Result<std::unique_ptr<NodeParamList>> parse_param_list(NodeAST* parent, bool allow_linebreaks = true);
	Result<std::unique_ptr<NodeAST>> parse_init_list(NodeAST* parent, bool allow_linebreaks = true);
    /// parses every expression from binary, string, unary to number and variable
    Result<std::unique_ptr<NodeAST>> parse_expression(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_string_expr(NodeAST* parent);
        /// Helper function for parsing binary string expression recursively
        Result<std::unique_ptr<NodeAST>> _parse_string_expr_rhs(std::unique_ptr<NodeAST> lhs, NodeAST* parent);
    /// parse unary or binary expression
    Result<std::unique_ptr<NodeAST>> parse_cast(std::unique_ptr<NodeAST> value, NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_binary_expr(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_unary_expr(NodeAST* parent);
	    /// Helper function for parsing binary expressions recursion
		Result<std::unique_ptr<NodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs, NodeAST* parent);
		/// Helper function for parsing ternary tail
		Result<std::unique_ptr<NodeAST>> _parse_ternary_rhs(std::unique_ptr<NodeAST> condition, NodeAST* parent);
		Result<std::unique_ptr<NodeAST>> _parse_null_coalesce_rhs(std::unique_ptr<NodeAST> lhs, NodeAST* parent);
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
    Result<std::unique_ptr<NodeConst>> parse_const_statement(NodeAST* parent);
    Result<std::unique_ptr<NodeAST>> parse_list_block(NodeAST* parent);
	Result<std::unique_ptr<NodeAST>> parse_family_statement(NodeAST* parent);
	Result<std::unique_ptr<NodeStruct>> parse_struct(NodeAST* parent);
	Result<std::vector<Token>> parse_struct_type_parameters(const Token& struct_name);
	/// RAII struct scope while parsing for the parameterized types of generic structs
	class StructTypeScope final {
		std::vector<NodeStruct*>& m_stack;
	public:
		StructTypeScope(std::vector<NodeStruct*>& stack, NodeStruct* strct) : m_stack(stack) {
			m_stack.push_back(strct);
		}
		~StructTypeScope() {
			m_stack.pop_back();
		}
		StructTypeScope(const StructTypeScope&) = delete;
		StructTypeScope& operator=(const StructTypeScope&) = delete;
	};

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
	/// Engaged while the body of a function whose result is named <return> is being parsed.
	/// SublimeKSP has no <return> statement, so <function neg(x) -> return> spells the result
	/// with a word CKSP reserves - see parse_function_definition, which already takes it as a
	/// name in the header, and parse_statement, which has to take it as one in the body too.
	bool m_result_named_return = false;
	/// Engaged while a SublimeKSP <taskfunc> block is being parsed, so its parameters accept
	/// the <var>/<out> modifiers and the edits reach the migration diagnostic. Owned rather
	/// than pointed at because the block's error paths return without unwinding through here.
	std::optional<TaskfuncMigration> m_taskfunc_migration;
    /// function params are no references -> replace with references
    Result<std::unique_ptr<NodeParamList>> parse_function_args(NodeAST* parent);
	Result<std::unique_ptr<NodeFunctionHeader>> parse_function_header(NodeAST* parent);
	Result<std::unique_ptr<NodeFunctionHeaderRef>> parse_function_header_ref(NodeAST* parent);
    Result<std::unique_ptr<NodeFunctionCall>> parse_function_call(NodeAST* parent);
    Result<std::unique_ptr<NodeCallback>> parse_callback(NodeAST* parent);
	Result<std::unique_ptr<NodeNamespace>> parse_namespace(NodeAST* parent);

	Result<std::unique_ptr<NodeProgram>> parse_program();

	Result<SuccessTag> consume_linebreak(const std::string& construct);
};
