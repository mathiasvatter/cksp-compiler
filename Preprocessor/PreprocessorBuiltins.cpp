//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreprocessorBuiltins.h"
#include "engine_widgets.h"
#include "engine_variables.h"
#include "engine_functions.h"

PreprocessorBuiltins::PreprocessorBuiltins()
: Preprocessor(std::vector<Token>{}, (std::string)"") {
    m_pos = 0;
//    m_builtin_variables_file = builtin_vars;
//    m_builtin_functions_file = builtin_functions;
//	m_builtin_widgets_file = builtin_widgets;
    m_builtin_variables_file = "engine_variables.h";
    m_builtin_functions_file = "engine_functions.h";
	m_builtin_widgets_file = "engine_widgets.h";
}

void PreprocessorBuiltins::process_builtins() {
    auto builtin_vars = parse_builtin_variables(m_builtin_variables_file);
    if(builtin_vars.is_error())
        builtin_vars.get_error().exit();

    auto builtin_functions = parse_builtin_functions(m_builtin_functions_file);
    if(builtin_functions.is_error())
        builtin_functions.get_error().exit();

	auto builtin_widgets = parse_builtin_widgets(m_builtin_widgets_file);
	if(builtin_widgets.is_error())
		builtin_widgets.get_error().exit();
}


Result<SuccessTag> PreprocessorBuiltins::parse_builtin_variables(const std::string &file) {
    std::string data(reinterpret_cast<char*>(engine_variables), engine_variables_len);
    Tokenizer tokenizer(file);
    tokenizer.set_input(data);
    m_tokens = tokenizer.tokenize();
    m_pos = 0;
    while(peek(m_tokens).type != END_TOKEN) {
        if(peek(m_tokens).type == KEYWORD) {
            if(contains(VAR_IDENT, peek(m_tokens).val[0])) {
                auto node_variable = std::move(parse_builtin_variable());
                m_builtin_variables.insert({node_variable->name, std::move(node_variable)});
            } else if(contains(ARRAY_IDENT, peek(m_tokens).val[0])) {
                auto node_array = std::move(parse_builtin_array());
                m_builtin_arrays.insert({node_array->name, std::move(node_array)});
            } else {
                return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
               "Failed loading builtins. Found builtin variable without identifier.", peek(m_tokens).line, "<identifier>", peek(m_tokens).val, peek(m_tokens).file));
            }
        } else consume(m_tokens);
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorBuiltins::parse_builtin_functions(const std::string &file) {
    std::string data(reinterpret_cast<char*>(engine_functions), engine_functions_len);
    Tokenizer tokenizer(file);
    tokenizer.set_input(data);
    m_tokens = tokenizer.tokenize();
    m_pos = 0;
    while(peek(m_tokens).type != END_TOKEN) {
        if(peek(m_tokens).type == KEYWORD) {
            auto result_function = parse_builtin_function();
            if(result_function.is_error()) {
                return Result<SuccessTag>(result_function.get_error());
            }
            if(is_property_function(result_function.unwrap()->name)) {
                m_property_functions.insert({result_function.unwrap()->name, std::move(result_function.unwrap())});
            } else {
                auto node_function = std::move(result_function.unwrap());
                m_builtin_functions[{node_function->name, (int)node_function->args->params.size()}] = std::move(node_function);
            }
        } else consume(m_tokens);
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorBuiltins::parse_builtin_widgets(const std::string &file) {
    std::string data(reinterpret_cast<char*>(engine_widgets), engine_widgets_len);
	Tokenizer tokenizer(file);
    tokenizer.set_input(data);
	m_tokens = tokenizer.tokenize();
	m_pos = 0;
	while(peek(m_tokens).type != END_TOKEN) {
		if(peek(m_tokens).type == UI_CONTROL) {
			auto result_ui_control = parse_builtin_ui_control();
			if(result_ui_control.is_error()) {
				return Result<SuccessTag>(result_ui_control.get_error());
			}
			m_builtin_widgets.insert({result_ui_control.unwrap()->ui_control_type, std::move(result_ui_control.unwrap())});
		} else consume(m_tokens);
	}
	return Result<SuccessTag>(SuccessTag{});
}


std::unique_ptr<NodeVariable> PreprocessorBuiltins::parse_builtin_variable() {
    Token name = consume(m_tokens); // consume variable name token
    // cut away identifier
    std::string var_name = name.val;
    ASTType type = get_identifier_type(var_name[0]);
    if(get_token_type(TYPES, std::string(1, var_name[0])))
        var_name = var_name.erase(0,1);
    auto node_variable = std::make_unique<NodeVariable>(false, var_name, Mutable, name);
    node_variable->type = type;
    node_variable->is_local = false;
    node_variable->is_engine = true;
    return std::move(node_variable);
}

std::unique_ptr<NodeArray> PreprocessorBuiltins::parse_builtin_array() {
    Token name = consume(m_tokens); // consume array name token
    std::string arr_name = name.val;
    ASTType type = get_identifier_type(arr_name[0]);
    if(get_token_type(TYPES, std::string(1, arr_name[0])))
        arr_name = arr_name.erase(0,1);
    std::unique_ptr<NodeParamList> size = std::unique_ptr<NodeParamList>(new NodeParamList({}, name));;
    std::unique_ptr<NodeParamList> index = std::unique_ptr<NodeParamList>(new NodeParamList({}, name));;
    auto node_array = std::make_unique<NodeArray>(false, arr_name, Array, std::move(size), std::move(index), name);
    node_array->type = type;
    node_array->is_local = false;
    node_array->is_engine = true;
    return std::move(node_array);
}

ASTType PreprocessorBuiltins::get_identifier_type(char identifier) {
    token token_type = *get_token_type(TYPES, std::string(1, identifier));
    return token_to_type(token_type);
}

Result<std::unique_ptr<NodeFunctionHeader>> PreprocessorBuiltins::parse_builtin_function() {
    Token func_name = consume(m_tokens); // consume function name
    std::unique_ptr<NodeParamList> func_args = std::unique_ptr<NodeParamList>(new NodeParamList({}, func_name));
    std::vector<ASTType> arg_types;
    std::vector<VarType> arg_var_types;
    bool has_forced_parenth = false;
    if (peek(m_tokens).type == token::OPEN_PARENTH) {
        has_forced_parenth = true;
        consume(m_tokens); // consume (
        if(peek(m_tokens).type != token::CLOSED_PARENTH) {
            auto arg_pair = parse_builtin_args_list(func_args);
            if (arg_pair.is_error()) {
                Result<std::unique_ptr<NodeFunctionHeader>>(arg_pair.get_error());
            }
            arg_types = arg_pair.unwrap().first;
            arg_var_types = arg_pair.unwrap().second;
        }
        if (peek(m_tokens).type == token::CLOSED_PARENTH) {
            consume(m_tokens);
        } else {
            return Result<std::unique_ptr<NodeFunctionHeader>>(CompileError(ErrorType::PreprocessorError,
           "Failed loading builtins. Found unknown <function_header> syntax.", peek(m_tokens).line, ")", peek(m_tokens).val, peek(m_tokens).file));
        }
    }

    ASTType return_type = Void;
    if(peek(m_tokens).type == TYPE) {
        consume(m_tokens); // consume :
        return_type = get_type_annotation(consume(m_tokens));
    }
    auto node_function = std::make_unique<NodeFunctionHeader>(func_name.val, std::move(func_args), func_name);
    node_function->type = return_type;
    node_function->is_engine = true;
    node_function->has_forced_parenth = has_forced_parenth;
    node_function->arg_var_types = arg_var_types;
    node_function->arg_ast_types = arg_types;
    return Result<std::unique_ptr<NodeFunctionHeader>>(std::move(node_function));
}

Result<std::unique_ptr<NodeUIControl>> PreprocessorBuiltins::parse_builtin_ui_control() {
	Token tok = consume(m_tokens);
	std::string ui_control_type = tok.val; // consume ui_control identifier
	if(peek(m_tokens).type != KEYWORD) {
		return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::PreprocessorError,
		"Failed loading builtins. Found unknown <engine_widget> syntax.", peek(m_tokens).line, "<Keyword>", peek(m_tokens).val, peek(m_tokens).file));
	}
	std::unique_ptr<NodeAST> node_var;
	if(peek(m_tokens, 1).type == OPEN_BRACKET) {
		node_var = std::move(parse_builtin_array());
		consume(m_tokens); // consume open bracket
		if(peek(m_tokens).type == KEYWORD) consume(m_tokens);
		if(peek(m_tokens).type == CLOSED_BRACKET) consume(m_tokens);
	} else {
		node_var = parse_builtin_variable();
	}
	std::unique_ptr<NodeParamList> params = std::unique_ptr<NodeParamList>(new NodeParamList({}, tok));
	std::vector<ASTType> arg_types;
	std::vector<VarType> arg_var_types;
	if (peek(m_tokens).type == token::OPEN_PARENTH) {
		consume(m_tokens); // consume (
		if(peek(m_tokens).type != token::CLOSED_PARENTH) {
			auto arg_pair = parse_builtin_args_list(params);
			if (arg_pair.is_error()) {
				Result<std::unique_ptr<NodeFunctionHeader>>(arg_pair.get_error());
			}
			arg_types = arg_pair.unwrap().first;
			arg_var_types = arg_pair.unwrap().second;
		}
		if (peek(m_tokens).type == token::CLOSED_PARENTH) {
			consume(m_tokens);
		} else {
			return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::PreprocessorError,
			"Failed loading builtins. Found unknown <engine_widget> parameter syntax.", peek(m_tokens).line, ")", peek(m_tokens).val, peek(m_tokens).file));
		}
	}
	auto node_ui_control = std::make_unique<NodeUIControl>(ui_control_type, std::move(node_var), std::move(params), tok);
	node_ui_control->control_var->parent = node_ui_control.get();
	node_ui_control->params->parent = node_ui_control.get();
	node_ui_control->arg_var_types = std::move(arg_var_types);
	node_ui_control->arg_ast_types = std::move(arg_types);
	node_ui_control->type = node_ui_control->control_var->type;
	return Result<std::unique_ptr<NodeUIControl>>(std::move(node_ui_control));
}

ASTType PreprocessorBuiltins::get_type_annotation(const Token& tok) {
//    Token token_type = tok; // get type token
    ASTType type = Any;
    if(tok.val.find("int") != std::string::npos) {
        type = Integer;
    } else if (tok.val.find("real") != std::string::npos) {
        type = Real;
    } else if (tok.val.find("string") != std::string::npos) {
        type = String;
    } else if(tok.val.find("number") != std::string::npos) {
        type = Number;
    } else if(tok.val.find("bool") != std::string::npos) {
        type = Boolean;
    }
    return type;
}

VarType PreprocessorBuiltins::get_var_type_annotation(const std::string& keyword) {
    if(keyword.find("array") != std::string::npos) {
        return Array;
    }
    return Mutable;
}

Result<std::pair<std::vector<ASTType>, std::vector<VarType>>> PreprocessorBuiltins::parse_builtin_args_list(std::unique_ptr<NodeParamList>& func_args) {
    std::vector<ASTType> arg_types;
    std::vector<VarType> arg_var_types;
    while(peek(m_tokens).type != CLOSED_PARENTH) {
        if(peek(m_tokens).type == CLOSED_PARENTH) break;
        if(peek(m_tokens).type == KEYWORD or peek(m_tokens).type == TO) {
            Token tok = peek(m_tokens);
            auto arg = parse_builtin_variable();
            func_args->params.push_back(std::move(arg));
            arg_var_types.push_back(get_var_type_annotation(tok.val));
            if(peek(m_tokens).type == TYPE) {
                consume(m_tokens); // consume semicolon
                if(peek(m_tokens).type != KEYWORD) {
                    return Result<std::pair<std::vector<ASTType>, std::vector<VarType>>>(CompileError(ErrorType::PreprocessorError,
                      "Failed loading builtins. Found unknown syntax in function arguments.", peek(m_tokens).line, "", peek(m_tokens).val, peek(m_tokens).file));
                }
                tok = consume(m_tokens);
            }
            arg_types.push_back(get_type_annotation(tok));
        } else {
            return Result<std::pair<std::vector<ASTType>, std::vector<VarType>>>(CompileError(ErrorType::PreprocessorError,
        "Failed loading builtins. Found unknown syntax in function arguments.", peek(m_tokens).line, "", peek(m_tokens).val, peek(m_tokens).file));
        }
        if(peek(m_tokens).type == COMMA) consume(m_tokens); // consume comma
    }
    auto result_pair = std::pair(arg_types, arg_var_types);
    return Result<std::pair<std::vector<ASTType>, std::vector<VarType>>>(result_pair);
}

const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &PreprocessorBuiltins::get_builtin_variables() const {
    return m_builtin_variables;
}

const std::unordered_map<std::string, std::unique_ptr<NodeArray>> &PreprocessorBuiltins::get_builtin_arrays() const {
    return m_builtin_arrays;
}

const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &PreprocessorBuiltins::get_builtin_functions() const {
    return m_builtin_functions;
}

const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &PreprocessorBuiltins::get_property_functions() const {
    return m_property_functions;
}

const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &PreprocessorBuiltins::get_builtin_widgets() const {
	return m_builtin_widgets;
}

bool PreprocessorBuiltins::is_property_function(const std::string &fun_name) {
    return contains(fun_name, "_properties") || contains(fun_name, "set_bounds");
}


