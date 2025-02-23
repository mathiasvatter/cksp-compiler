//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "../Processor/Processor.h"
#include "DefinitionProvider.h"

class BuiltinsProcessor : public Processor {
public:
	explicit BuiltinsProcessor(DefinitionProvider* definition_provider);

	/// main function to process the tokens and parse the builtins
    void process() override;

    Result<SuccessTag> parse_builtin_variables(const std::string &file);
    Result<SuccessTag> parse_builtin_functions(const std::string &file);
	Result<SuccessTag> parse_builtin_widgets(const std::string &file);

    Result<std::shared_ptr<NodeVariable>> parse_builtin_variable();
    Result<std::shared_ptr<NodeArray>> parse_builtin_array();
    Result<std::shared_ptr<NodeFunctionDefinition>> parse_builtin_function();
	Result<std::shared_ptr<NodeUIControl>> parse_builtin_ui_control();
    Result<std::unique_ptr<NodeParamList>> parse_builtin_args_list();
	Result<std::vector<std::unique_ptr<NodeFunctionParam>>> parse_builtin_params_list();

private:
	DefinitionProvider* m_def_provider;
    std::unordered_map<std::string, std::shared_ptr<NodeVariable>> m_builtin_variables;
    std::unordered_map<std::string, std::shared_ptr<NodeArray>> m_builtin_arrays;
    std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_builtin_functions;

    std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> m_property_functions;
    std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> m_builtin_widgets;

    std::string m_builtin_variables_file;
    std::string m_builtin_functions_file;
	std::string m_builtin_widgets_file;

	static bool is_threadsafe_function(const std::string& fun_name) {
		static const std::set<std::string> thread_unsafe_functions = {"wait", "wait_ticks", "wait_async"};
		return !thread_unsafe_functions.contains(fun_name);
	}

	static bool is_restricted_function(const std::string& fun_name) {
		static const std::unordered_set<std::string> restricted_functions = {"save_array", "save_array_str", "load_array", "load_array_str", "purge_group"};
		return restricted_functions.contains(fun_name);
	}

//	static void apply_builtin_information(NodeDataStructure* node);

	static void apply_annotation_information(NodeDataStructure* node);

//    static DataType get_var_type_annotation(const std::string& keyword);
    static bool is_property_function(const std::string& fun_name);
};

