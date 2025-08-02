//
// Created by Mathias Vatter on 24.01.24.
//

#pragma once

#include "PreASTVisitor.h"
#include "../ImportProcessor.h"
#include "../../misc/CommandLineOptions.h"
#include "../../misc/PathHandler.h"


class PreASTPragma final : public PreASTVisitor {
	CompilerConfig* m_config = nullptr;
	std::unordered_map<std::string, std::function<void(const std::string&, const Token&)>> pragma_handlers{};

public:
	explicit PreASTPragma(CompilerConfig* config) : m_config(config) {
		register_pragma_handlers();
	}

	PreNodeAST *visit(PreNodePragma &node) override {
		const std::string& option = node.option->get_string();
		const Token& token = node.argument->value;

		auto it = pragma_handlers.find(option);
		if (it == pragma_handlers.end()) {
			get_pragma_error(token, option, "valid <#pragma> option.").exit();
		}
		it->second(node.argument->value.val, token);
		return &node;
	}

	PreNodeAST *visit(PreNodeProgram &node) override {
		m_program = &node;
		visit_all(node.program, *this);
		return &node;
	}

private:

	void register_pragma_handlers() {
		pragma_handlers["output_path"] = [this](const std::string& arg, const Token& token) {
			auto path = StringUtils::remove_quotes(arg);

			std::string error_message = "Found unknown <output_path> option in <#pragma>. ";
			static PathHandler path_handler(token, token.file, "");
			auto output_path = path_handler.resolve_path(path);
			if (output_path.is_error()) {
				auto error = output_path.get_error();
				error.m_message.insert(0, error_message);
				error.exit();
			}
			auto valid_output_path = path_handler.generate_output_file(output_path.unwrap());
			if (valid_output_path.is_error()) {
				auto error = valid_output_path.get_error();
				error.m_message.insert(0, error_message);
				error.exit();
			}
			m_config->output_filename = valid_output_path.unwrap();
		};

		pragma_handlers["optimize"] = [this](const std::string& arg, const Token& token) {
			std::string level = StringUtils::remove_quotes(arg);

			const auto it = optimization_level_map.find(level);
			if (it == optimization_level_map.end()) {
				get_pragma_error(token, level, "'none', 'simple', 'standard', or 'aggressive'").exit();
			}
			m_config->optimization_level = it->second;
		};

		pragma_handlers["pass_by"] = [this](const std::string& arg, const Token& token) {
			std::string pass_by = StringUtils::remove_quotes(arg);

			const auto it = parameter_passing_map.find(pass_by);
			if (it == parameter_passing_map.end()) {
				get_pragma_error(token, pass_by, "'value' or 'reference'").exit();
			}
			m_config->parameter_passing = it->second;
		};
	}

	static CompileError get_pragma_error(const Token& tok, const std::string& got, const std::string& expected) {
		auto err = CompileError(ErrorType::PreprocessorError, "", "", tok);
		err.m_message = "Found unknown <#pragma> option or value.";
		err.m_expected = expected;
		err.m_got = got;
		return err;
	}

};
