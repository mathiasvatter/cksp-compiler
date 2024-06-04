//
// Created by Mathias Vatter on 18.11.23.
//

#include "BuiltinsProcessor.h"

#include <memory>
#include "engine_widgets.h"
#include "engine_variables.h"
#include "engine_functions.h"

BuiltinsProcessor::BuiltinsProcessor(DefinitionProvider* definition_provider)
: Processor(), m_def_provider(definition_provider) {
    m_pos = 0;
    m_builtin_variables_file = "engine_variables.h";
    m_builtin_functions_file = "engine_functions.h";
	m_builtin_widgets_file = "engine_widgets.h";
}

void BuiltinsProcessor::process() {
    auto builtin_vars = parse_builtin_variables(m_builtin_variables_file);
    if(builtin_vars.is_error())
        builtin_vars.get_error().exit();

    auto builtin_functions = parse_builtin_functions(m_builtin_functions_file);
    if(builtin_functions.is_error())
        builtin_functions.get_error().exit();

	auto builtin_widgets = parse_builtin_widgets(m_builtin_widgets_file);
	if(builtin_widgets.is_error())
		builtin_widgets.get_error().exit();

	m_def_provider->set_builtin_variables(std::move(m_builtin_variables));
	m_def_provider->set_builtin_arrays(std::move(m_builtin_arrays));
	m_def_provider->set_builtin_functions(std::move(m_builtin_functions));
	m_def_provider->set_builtin_widgets(std::move(m_builtin_widgets));
	m_def_provider->set_property_functions(std::move(m_property_functions));
}


Result<SuccessTag> BuiltinsProcessor::parse_builtin_variables(const std::string &file) {
    std::string data(reinterpret_cast<char*>(engine_variables), engine_variables_len);
    Tokenizer tokenizer(data, file);
    m_tokens = tokenizer.tokenize();
    m_pos = 0;
    while(peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::KEYWORD) {
            if(contains(VAR_IDENT, peek(m_tokens).val[0])) {
				auto node_variable_res = parse_builtin_variable();
				if(node_variable_res.is_error()) {
					return Result<SuccessTag>(node_variable_res.get_error());
				}
                auto node_variable = std::move(node_variable_res.unwrap());
                m_builtin_variables.insert({node_variable->name, std::move(node_variable)});
            } else if(contains(ARRAY_IDENT, peek(m_tokens).val[0])) {
				auto node_array_res = parse_builtin_array();
				if(node_array_res.is_error()) {
					return Result<SuccessTag>(node_array_res.get_error());
				}
				auto node_array = std::move(node_array_res.unwrap());
                m_builtin_arrays.insert({node_array->name, std::move(node_array)});
            } else if(contains(BUILTIN_CONDITIONS, peek(m_tokens).val)) {
				auto node_variable_res = parse_builtin_variable();
				if(node_variable_res.is_error()) {
					return Result<SuccessTag>(node_variable_res.get_error());
				}
				auto node_variable = std::move(node_variable_res.unwrap());
				m_builtin_variables.insert({node_variable->name, std::move(node_variable)});
			} else {
                return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
               "Failed loading builtins. Found builtin variable without identifier.", peek(m_tokens).line, "<identifier>", peek(m_tokens).val, peek(m_tokens).file));
            }
        } else consume(m_tokens);
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> BuiltinsProcessor::parse_builtin_functions(const std::string &file) {
    std::string data(reinterpret_cast<char*>(engine_functions), engine_functions_len);
    Tokenizer tokenizer(data, file);
    m_tokens = tokenizer.tokenize();
    m_pos = 0;
    while(peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::KEYWORD || peek(m_tokens).type == token::SET_CONDITION || peek(m_tokens).type == token::RESET_CONDITION) {
            auto result_function = parse_builtin_function();
            if(result_function.is_error()) {
                return Result<SuccessTag>(result_function.get_error());
            }
            if(is_property_function(result_function.unwrap()->header->name)) {
                m_property_functions.insert({result_function.unwrap()->header->name, std::move(result_function.unwrap())});
            } else {
                auto node_function = std::move(result_function.unwrap());
                m_builtin_functions[{node_function->header->name, (int)node_function->header->args->params.size()}] = std::move(node_function);
            }
        } else consume(m_tokens);
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> BuiltinsProcessor::parse_builtin_widgets(const std::string &file) {
    std::string data(reinterpret_cast<char*>(engine_widgets), engine_widgets_len);
	Tokenizer tokenizer(data, file);
	m_tokens = tokenizer.tokenize();
	m_pos = 0;
	while(peek(m_tokens).type != token::END_TOKEN) {
		if(peek(m_tokens).type == token::UI_CONTROL) {
			auto result_ui_control = parse_builtin_ui_control();
			if(result_ui_control.is_error()) {
				return Result<SuccessTag>(result_ui_control.get_error());
			}
			m_builtin_widgets.insert({result_ui_control.unwrap()->ui_control_type, std::move(result_ui_control.unwrap())});
		} else consume(m_tokens);
	}
	return Result<SuccessTag>(SuccessTag{});
}


Result<std::unique_ptr<NodeVariable>> BuiltinsProcessor::parse_builtin_variable() {
    Token name = consume(m_tokens); // consume variable name token
    // cut away identifier
    std::string var_name = name.val;
//    ASTType type = get_identifier_type(var_name[0]);
	Type* ty = TypeRegistry::get_type_from_identifier(var_name[0]);
    if(get_token_type(TYPES, std::string(1, var_name[0])))
        var_name = var_name.erase(0,1);

	auto type_annotation = parse_type_annotation(ty);
	if(type_annotation.is_error()) {
		return Result<std::unique_ptr<NodeVariable>>(type_annotation.get_error());
	}
    auto node_variable = std::make_unique<NodeVariable>(std::optional<Token>(), var_name, type_annotation.unwrap(), DataType::Mutable, name);
//    node_variable->type = type;
    node_variable->is_local = false;
    node_variable->is_engine = true;
    return Result<std::unique_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::unique_ptr<NodeArray>> BuiltinsProcessor::parse_builtin_array() {
    Token name = consume(m_tokens); // consume array name token
    std::string arr_name = name.val;
//    ASTType type = get_identifier_type(arr_name[0]);
	Type* ty = TypeRegistry::get_type_from_identifier(arr_name[0]);
    if(get_token_type(TYPES, std::string(1, arr_name[0])))
        arr_name = arr_name.erase(0,1);
	auto type_annotation = parse_type_annotation(ty);
	if(type_annotation.is_error()) {
		return Result<std::unique_ptr<NodeArray>>(type_annotation.get_error());
	}
    auto node_array = std::make_unique<NodeArray>(
		std::nullopt,
		arr_name,
		type_annotation.unwrap(),
		DataType::Array,
		std::make_unique<NodeParamList>(name), name
	);
//    node_array->type = type;
    node_array->is_local = false;
    node_array->is_engine = true;
    return Result<std::unique_ptr<NodeArray>>(std::move(node_array));
}

ASTType BuiltinsProcessor::get_identifier_type(char identifier) {
    token token_type = *get_token_type(TYPES, std::string(1, identifier));
    return token_to_type(token_type);
}

Result<std::unique_ptr<NodeFunctionDefinition>> BuiltinsProcessor::parse_builtin_function() {
    Token func_name = consume(m_tokens); // consume function name
    std::unique_ptr<NodeParamList> func_args = std::make_unique<NodeParamList>(func_name);
    std::vector<ASTType> arg_types;
    std::vector<DataType> arg_var_types;
	std::vector<Type*> types;
    bool has_forced_parenth = false;
    if (peek(m_tokens).type == token::OPEN_PARENTH) {
        has_forced_parenth = true;
        consume(m_tokens); // consume (
        if(peek(m_tokens).type != token::CLOSED_PARENTH) {
            auto param_list = parse_builtin_args_list();
            if (param_list.is_error()) {
                Result<std::unique_ptr<NodeFunctionDefinition>>(param_list.get_error());
            }
            func_args = std::move(param_list.unwrap());
            for(int i = 0; i<func_args->params.size(); i++) {
                arg_types.push_back(func_args->params[i]->type);
                types.push_back(func_args->params[i]->ty);
            }
        }
        if (peek(m_tokens).type == token::CLOSED_PARENTH) {
            consume(m_tokens);
        } else {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::PreprocessorError,
           "Failed loading builtins. Found unknown <function_header> syntax.", peek(m_tokens).line, ")", peek(m_tokens).val, peek(m_tokens).file));
        }
    }

	Type* ret_type = TypeRegistry::Void;
    ASTType return_type = ASTType::Void;
    if(peek(m_tokens).type == token::TYPE) {
        consume(m_tokens); // consume :
		ret_type = TypeRegistry::get_type_from_annotation(peek(m_tokens).val);
        return_type = get_type_annotation(consume(m_tokens));
    }
    auto node_function_header = std::make_unique<NodeFunctionHeader>(
            func_name.val, std::move(func_args), func_name
        );
	node_function_header->is_thread_safe = m_thread_unsafe_functions.find(node_function_header->name) == m_thread_unsafe_functions.end();
	node_function_header->ty = ret_type;
    node_function_header->type = return_type;
    node_function_header->is_builtin = true;
    node_function_header->has_forced_parenth = has_forced_parenth;
    node_function_header->arg_var_types = arg_var_types;
    node_function_header->arg_ast_types = arg_types;
	node_function_header->arg_types = types;

    auto node_function = std::make_unique<NodeFunctionDefinition>(
            std::move(node_function_header),
            std::nullopt,
            false,
            std::make_unique<NodeBody>(func_name),
            func_name
            );
    node_function->ty = ret_type;
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(node_function));
}

Result<std::unique_ptr<NodeUIControl>> BuiltinsProcessor::parse_builtin_ui_control() {
	Token tok = consume(m_tokens);
	std::string ui_control_type = tok.val; // consume ui_control identifier
	if(peek(m_tokens).type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::PreprocessorError,
		"Failed loading builtins. Found unknown <engine_widget> syntax.", peek(m_tokens).line, "<Keyword>", peek(m_tokens).val, peek(m_tokens).file));
	}
	std::unique_ptr<NodeDataStructure> node_var;
	if(peek(m_tokens, 1).type == token::OPEN_BRACKET) {
		auto node_var_res = parse_builtin_array();
		if(node_var_res.is_error()) {
			return Result<std::unique_ptr<NodeUIControl>>(node_var_res.get_error());
		}
		node_var = std::move(node_var_res.unwrap());
		consume(m_tokens); // consume open bracket
		if(peek(m_tokens).type == token::KEYWORD) consume(m_tokens);
		if(peek(m_tokens).type == token::CLOSED_BRACKET) consume(m_tokens);
	} else {
		auto node_var_res = parse_builtin_variable();
		if(node_var_res.is_error()) {
			return Result<std::unique_ptr<NodeUIControl>>(node_var_res.get_error());
		}
		node_var = std::move(node_var_res.unwrap());
	}
	std::unique_ptr<NodeParamList> params = std::make_unique<NodeParamList>(tok);
	std::vector<ASTType> arg_types;
	std::vector<DataType> arg_var_types;
	std::vector<Type*> types;
	if (peek(m_tokens).type == token::OPEN_PARENTH) {
		consume(m_tokens); // consume (
		if(peek(m_tokens).type != token::CLOSED_PARENTH) {
			auto param_list = parse_builtin_args_list();
			if (param_list.is_error()) {
				Result<std::unique_ptr<NodeFunctionHeader>>(param_list.get_error());
			}
            params = std::move(param_list.unwrap());
            for(int i = 0; i<params->params.size(); i++) {
                arg_types.push_back(params->params[i]->type);
                types.push_back(params->params[i]->ty);
            }
		}
		if (peek(m_tokens).type == token::CLOSED_PARENTH) {
			consume(m_tokens);
		} else {
			return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::PreprocessorError,
			"Failed loading builtins. Found unknown <engine_widget> parameter syntax.", peek(m_tokens).line, ")", peek(m_tokens).val, peek(m_tokens).file));
		}
	}
	node_var->data_type = DataType::UI_Control;
	auto node_ui_control = std::make_unique<NodeUIControl>(ui_control_type, std::move(node_var), std::move(params), tok);
	node_ui_control->control_var->parent = node_ui_control.get();
	node_ui_control->params->parent = node_ui_control.get();
	node_ui_control->arg_var_types = std::move(arg_var_types);
	node_ui_control->arg_ast_types = std::move(arg_types);
	node_ui_control->arg_types = types;
	node_ui_control->type = node_ui_control->control_var->type;
	node_ui_control->ty = node_ui_control->control_var->ty;
	return Result<std::unique_ptr<NodeUIControl>>(std::move(node_ui_control));
}

ASTType BuiltinsProcessor::get_type_annotation(const Token& tok) {
//    Token token_type = tok; // get type token
    ASTType type = ASTType::Any;
    if(tok.val.find("int") != std::string::npos) {
        type = ASTType::Integer;
    } else if (tok.val.find("real") != std::string::npos) {
        type = ASTType::Real;
    } else if (tok.val.find("string") != std::string::npos) {
        type = ASTType::String;
    } else if(tok.val.find("number") != std::string::npos) {
        type = ASTType::Number;
    } else if(tok.val.find("bool") != std::string::npos) {
        type = ASTType::Boolean;
    }
    return type;
}

DataType BuiltinsProcessor::get_var_type_annotation(const std::string& keyword) {
    if(keyword.find("array") != std::string::npos) {
        return DataType::Array;
    }
    return DataType::Mutable;
}

Result<std::unique_ptr<NodeParamList>> BuiltinsProcessor::parse_builtin_args_list() {
//    std::vector<ASTType> arg_types;
//    std::vector<DataType> arg_var_types;
	std::vector<Type*> types;
    auto func_args = std::make_unique<NodeParamList>(peek(m_tokens));
    while(peek(m_tokens).type != token::CLOSED_PARENTH) {
        if(peek(m_tokens).type == token::CLOSED_PARENTH) break;
        if(peek(m_tokens).type == token::KEYWORD or peek(m_tokens).type == token::TO) {
            Token tok = peek(m_tokens);
			auto node_arg_res = parse_builtin_variable();
			if(node_arg_res.is_error()) {
				return Result<std::unique_ptr<NodeParamList>>(node_arg_res.get_error());
			}
			auto arg = std::move(node_arg_res.unwrap());
            arg->parent = func_args.get();
			types.push_back(arg->ty);
            func_args->params.push_back(std::move(arg));
//            arg_var_types.push_back(get_var_type_annotation(tok.val));
//            if(peek(m_tokens).type == token::TYPE) {
//                consume(m_tokens); // consume semicolon
//                if(peek(m_tokens).type != token::KEYWORD) {
//                    return Result<std::unique_ptr<NodeParamList>>(CompileError(ErrorType::PreprocessorError,
//                      "Failed loading builtins. Found unknown syntax in function arguments.", peek(m_tokens).line, "", peek(m_tokens).val, peek(m_tokens).file));
//                }
//                tok = consume(m_tokens);
//            }
//			auto ty = parse_type_annotation();
//			if(ty.is_error()) {
//				return Result<std::unique_ptr<NodeParamList>>(ty.get_error());
//			}
//            arg_types.push_back(get_type_annotation(tok));
        } else {
            return Result<std::unique_ptr<NodeParamList>>(CompileError(ErrorType::PreprocessorError,
                                                                                               "Failed loading builtins. Found unknown syntax in function arguments.", peek(m_tokens).line, "", peek(m_tokens).val, peek(m_tokens).file));
        }
        if(peek(m_tokens).type == token::COMMA) consume(m_tokens); // consume comma
    }
//    auto result_pair = std::tuple(types, arg_types, arg_var_types);
    return Result<std::unique_ptr<NodeParamList>>(std::move(func_args));
}

bool BuiltinsProcessor::is_property_function(const std::string &fun_name) {
    return contains(fun_name, "_properties") || contains(fun_name, "set_bounds");
}


