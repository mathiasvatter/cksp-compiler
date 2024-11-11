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
                m_builtin_functions[{node_function->header->name, (int)node_function->header->params.size()}] = std::move(node_function);
            }
        } else consume(m_tokens);
    }
    return Result<SuccessTag>(SuccessTag{});
}

void BuiltinsProcessor::apply_annotation_information(NodeDataStructure* node) {
	if(node->ty->get_type_kind() == TypeKind::Composite and node->get_node_type() == NodeType::Variable) {
		auto node_var = static_cast<NodeVariable*>(node);
		auto comp_type = static_cast<CompositeType*>(node->ty);
		if(comp_type->get_compound_type() == CompoundKind::Array) {
			auto node_array = static_cast<NodeVariable*>(node)->to_array(nullptr);
			node_var->replace_with(std::move(node_array));
		}
	}
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


Result<std::shared_ptr<NodeVariable>> BuiltinsProcessor::parse_builtin_variable() {
    Token name = consume(m_tokens); // consume variable name token
    // cut away identifier
    std::string var_name = name.val;
	Type* ty = TypeRegistry::get_type_from_identifier(var_name[0]);
    if(get_token_type(TYPES, std::string(1, var_name[0])))
        var_name = var_name.erase(0,1);

	auto type_annotation = parse_type_annotation(ty);
	if(type_annotation.is_error()) {
		return Result<std::shared_ptr<NodeVariable>>(type_annotation.get_error());
	}
    auto node_variable = std::make_shared<NodeVariable>(std::optional<Token>(), var_name, type_annotation.unwrap(), DataType::Mutable, name);
    node_variable->is_local = false;
    node_variable->is_engine = true;
    return Result<std::shared_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::shared_ptr<NodeArray>> BuiltinsProcessor::parse_builtin_array() {
    Token name = consume(m_tokens); // consume array name token
    std::string arr_name = name.val;
	Type* ty = TypeRegistry::get_type_from_identifier(arr_name[0]);
    if(get_token_type(TYPES, std::string(1, arr_name[0])))
        arr_name = arr_name.erase(0,1);
	auto type_annotation = parse_type_annotation(ty);
	if(type_annotation.is_error()) {
		return Result<std::shared_ptr<NodeArray>>(type_annotation.get_error());
	}
    auto node_array = std::make_shared<NodeArray>(
		std::nullopt,
		arr_name,
		type_annotation.unwrap(),
		std::make_unique<NodeParamList>(name), name
	);
    node_array->is_local = false;
    node_array->is_engine = true;
    return Result<std::shared_ptr<NodeArray>>(std::move(node_array));
}

Result<std::unique_ptr<NodeFunctionDefinition>> BuiltinsProcessor::parse_builtin_function() {
    Token func_name = consume(m_tokens); // consume function name
    std::vector<std::unique_ptr<NodeFunctionParam>> func_args;
	std::vector<Type*> types;
    bool has_forced_parenth = false;
    if (peek(m_tokens).type == token::OPEN_PARENTH) {
        has_forced_parenth = true;
        consume(m_tokens); // consume (
        if(peek(m_tokens).type != token::CLOSED_PARENTH) {
            auto param_list = parse_builtin_params_list();
            if (param_list.is_error()) {
                Result<std::unique_ptr<NodeFunctionDefinition>>(param_list.get_error());
            }
            func_args = std::move(param_list.unwrap());
            for(const auto & param : func_args) {
                types.push_back(param->variable->ty);
            }
        }
        if (peek(m_tokens).type == token::CLOSED_PARENTH) {
            consume(m_tokens);
        } else {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::PreprocessorError,
           "Failed loading builtins. Found unknown <function_header> syntax.", peek(m_tokens).line, ")", peek(m_tokens).val, peek(m_tokens).file));
        }
    }
	for(auto & arg : func_args) {
		apply_annotation_information(arg->variable.get());
	}

	int num_return_vars = 0;
	Type* ret_type = TypeRegistry::Void;
    if(peek(m_tokens).type == token::TYPE) {
        consume(m_tokens); // consume :
		ret_type = TypeRegistry::get_type_from_annotation(peek(m_tokens).val);
		num_return_vars = 1;
    }
    auto node_function_header = std::make_unique<NodeFunctionHeader>(
            func_name.val, std::move(func_args), func_name
        );
	node_function_header->create_function_type(ret_type);
    node_function_header->has_forced_parenth = has_forced_parenth;

    auto node_function = std::make_unique<NodeFunctionDefinition>(
            std::move(node_function_header),
            std::nullopt,
            false,
            std::make_unique<NodeBlock>(func_name),
            func_name
            );
	node_function->visited = true;
    node_function->ty = ret_type;
	node_function->is_thread_safe = is_threadsafe_function(node_function->header->name);
	node_function->is_restricted = is_restricted_function(node_function->header->name);
	node_function->num_return_params = num_return_vars;
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(node_function));
}

Result<std::shared_ptr<NodeUIControl>> BuiltinsProcessor::parse_builtin_ui_control() {
	Token tok = consume(m_tokens);
	std::string ui_control_type = tok.val; // consume ui_control identifier
	if(peek(m_tokens).type != token::KEYWORD) {
		return Result<std::shared_ptr<NodeUIControl>>(CompileError(ErrorType::PreprocessorError,
		"Failed loading builtins. Found unknown <engine_widget> syntax.", peek(m_tokens).line, "<Keyword>", peek(m_tokens).val, peek(m_tokens).file));
	}
	std::shared_ptr<NodeDataStructure> node_var;
	if(peek(m_tokens, 1).type == token::OPEN_BRACKET) {
		auto node_var_res = parse_builtin_array();
		if(node_var_res.is_error()) {
			return Result<std::shared_ptr<NodeUIControl>>(node_var_res.get_error());
		}
		node_var = std::move(node_var_res.unwrap());
		consume(m_tokens); // consume open bracket
		if(peek(m_tokens).type == token::KEYWORD) consume(m_tokens);
		if(peek(m_tokens).type == token::CLOSED_BRACKET) consume(m_tokens);
	} else {
		auto node_var_res = parse_builtin_variable();
		if(node_var_res.is_error()) {
			return Result<std::shared_ptr<NodeUIControl>>(node_var_res.get_error());
		}
		node_var = std::move(node_var_res.unwrap());
	}
	std::unique_ptr<NodeParamList> params = std::make_unique<NodeParamList>(tok);
	std::vector<Type*> types;
	if (peek(m_tokens).type == token::OPEN_PARENTH) {
		consume(m_tokens); // consume (
		if(peek(m_tokens).type != token::CLOSED_PARENTH) {
			auto param_list = parse_builtin_args_list();
			if (param_list.is_error()) {
				Result<std::shared_ptr<NodeFunctionHeader>>(param_list.get_error());
			}
            params = std::move(param_list.unwrap());
            for(const auto & param : params->params) {
                types.push_back(param->ty);
            }
		}
		if (peek(m_tokens).type == token::CLOSED_PARENTH) {
			consume(m_tokens);
		} else {
			return Result<std::shared_ptr<NodeUIControl>>(CompileError(ErrorType::PreprocessorError,
			"Failed loading builtins. Found unknown <engine_widget> parameter syntax.", peek(m_tokens).line, ")", peek(m_tokens).val, peek(m_tokens).file));
		}
	}
	node_var->data_type = DataType::UIControl;
	auto node_ui_control = std::make_shared<NodeUIControl>(ui_control_type, std::move(node_var), std::move(params), tok);
	node_ui_control->ty = node_ui_control->control_var->ty;
	return Result<std::shared_ptr<NodeUIControl>>(std::move(node_ui_control));
}

Result<std::unique_ptr<NodeParamList>> BuiltinsProcessor::parse_builtin_args_list() {
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
            func_args->params.push_back(arg->to_reference());
        } else {
            return Result<std::unique_ptr<NodeParamList>>(CompileError(ErrorType::PreprocessorError,
                                                                                               "Failed loading builtins. Found unknown syntax in function arguments.", peek(m_tokens).line, "", peek(m_tokens).val, peek(m_tokens).file));
        }
        if(peek(m_tokens).type == token::COMMA) consume(m_tokens); // consume comma
    }
    return Result<std::unique_ptr<NodeParamList>>(std::move(func_args));
}

Result<std::vector<std::unique_ptr<NodeFunctionParam>>> BuiltinsProcessor::parse_builtin_params_list() {
	std::vector<std::unique_ptr<NodeFunctionParam>> func_params;
	while(peek(m_tokens).type != token::CLOSED_PARENTH) {
		if(peek(m_tokens).type == token::CLOSED_PARENTH) break;
		if(peek(m_tokens).type == token::KEYWORD or peek(m_tokens).type == token::TO) {
			Token tok = peek(m_tokens);
			auto node_arg_res = parse_builtin_variable();
			if(node_arg_res.is_error()) {
				return Result<std::vector<std::unique_ptr<NodeFunctionParam>>>(node_arg_res.get_error());
			}
			auto arg = std::move(node_arg_res.unwrap());
			func_params.push_back(std::make_unique<NodeFunctionParam>(std::move(arg), nullptr, tok));
		} else {
			return Result<std::vector<std::unique_ptr<NodeFunctionParam>>>(CompileError(ErrorType::PreprocessorError,
			"Failed loading builtins. Found unknown syntax in function parameters.", peek(m_tokens).line, "", peek(m_tokens).val, peek(m_tokens).file));
		}
		if(peek(m_tokens).type == token::COMMA) consume(m_tokens); // consume comma
	}
	return Result<std::vector<std::unique_ptr<NodeFunctionParam>>>(std::move(func_params));
}

bool BuiltinsProcessor::is_property_function(const std::string &fun_name) {
    return contains(fun_name, "_properties") || contains(fun_name, "set_bounds");
}


