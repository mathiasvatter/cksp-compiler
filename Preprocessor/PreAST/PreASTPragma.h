//
// Created by Mathias Vatter on 24.01.24.
//

#pragma once

#include "PreASTVisitor.h"
#include "../../AST/ASTVisitor/ASTKSPSyntaxCheck.h"
#include "../../misc/CommandLineOptions.h"
#include "../../misc/PathHandler.h"


class PreASTPragma final : public PreASTVisitor {
	CompilerConfig* m_config = nullptr;
	std::unordered_map<std::string, std::function<void(const std::string&, const Token&)>> pragma_handlers{};

public:
	explicit PreASTPragma(CompilerConfig* config) : m_config(config) {
		register_pragma_handlers();
	}

	PreNodeAST *visit(PreNodeProgram &node) override {
		m_program = &node;
		node.program->accept(*this);
		return &node;
	}

	PreNodeAST *visit(PreNodePragma &node) override {
		const std::string& option = node.option->get_string();
		const Token& token = node.argument->tok;

		auto it = pragma_handlers.find(option);
		if (it == pragma_handlers.end()) {
			get_pragma_error(token, option, "valid <#pragma> option.").exit();
		}
		it->second(node.argument->tok.val, token);
		return &node;
	}


	PreNodeAST *visit(PreNodeMacroDefinition &node) override {
		node.header->accept(*this);
		node.body->accept(*this);

		if(m_program) {
			auto node_macro_definition = std::make_unique<PreNodeMacroDefinition>(
				std::move(node.header),
				std::move(node.body),
				node.tok,
				node.parent
			);
			m_program->macro_definitions.push_back(std::move(node_macro_definition));
		}
		// replace node with dead code after incrementation
		return node.replace_with(std::make_unique<PreNodeDeadCode>(node.tok, node.parent));
	}

private:

	void register_pragma_handlers() {
		pragma_handlers["output_path"] = [this](const std::string& arg, const Token& token) {
			auto path = StringUtils::remove_quotes(arg);

			std::string error_message = "Found unknown <output_path> option in <#pragma>. ";
			PathHandler path_handler(token, token.file, "");
			auto output_path = path_handler.resolve_path(path);
			if (output_path.is_error()) {
				auto error = output_path.get_error();
				error.m_message.insert(0, error_message);
				error.exit();
			}
			auto valid_output_path = path_handler.check_valid_output_file(output_path.unwrap());
			if (valid_output_path.is_error()) {
				auto error = valid_output_path.get_error();
				error.m_message.insert(0, error_message);
				error.exit();
			}
			m_config->outputs.push_back(valid_output_path.unwrap());
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

		pragma_handlers["combine_callbacks"] = [this](const std::string& arg, const Token& token) {
			std::string combine = StringUtils::remove_quotes(arg);

			if (combine == "true") {
				m_config->combine_callbacks = true;
			} else if (combine == "false") {
				m_config->combine_callbacks = false;
			} else {
				get_pragma_error(token, combine, "'true' or 'false'").exit();
			}
		};

		pragma_handlers["max_callback_depth"] = [this](const std::string& arg, const Token& token) {
			std::string depth_str = StringUtils::remove_quotes(arg);
			try {
				int depth = std::stoi(depth_str);
				if (depth < 0) {
					throw std::invalid_argument("negative value");
				}
				m_config->max_callback_depth = depth;
			} catch (const std::invalid_argument&) {
				get_pragma_error(token, depth_str, "a non-negative integer").exit();
			} catch (const std::out_of_range&) {
				get_pragma_error(token, depth_str, "a valid integer within range").exit();
			}
			if (m_config->max_callback_depth > MAX_ARRAY_ELEMENTS) {
				auto error = get_pragma_error(token, depth_str, "a value less than " + std::to_string(MAX_ARRAY_ELEMENTS));
				error.m_message = "The specified <max_callback_depth> exceeds the maximum allowed limit.";
				error.exit();
			}
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
