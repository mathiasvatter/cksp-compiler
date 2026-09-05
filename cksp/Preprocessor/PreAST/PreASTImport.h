//
// Created by Mathias Vatter on 25.02.26.
//

#pragma once

#include <utility>

#include "PreASTVisitor.h"
#include "../../Source/ReferenceIndex.h"
#include "../../Source/SourceParser.h"
#include "../../../JSON/NCKPTranslator.h"

class PreASTImport final : public PreASTVisitor {
	std::string m_debug_token;
	SourceId m_current_source;
	SourceId m_root_source;
	SourceParser& m_parser;
	/// Every file already imported, with the alias it was imported under (empty for none). Keeping
	/// the alias is what makes a second import under a different one reportable; the map itself is
	/// what stops a circular import from looping.
	std::unordered_map<std::string, std::string> &m_imported_files;
	std::unordered_map<std::string, std::string> &m_basename_map; // Map to store basename to full path mapping
	ReferenceIndex* m_reference_index;

	static void wrap_imported_program_in_namespace(const PreNodeImport& import, PreNodeChunk& program) {
		if (!import.alias) return;

		Token namespace_token = import.tok;
		namespace_token.type = token::NAMESPACE;
		namespace_token.val = "namespace";

		Token alias_token = import.alias->tok;

		Token opening_linebreak = alias_token;
		opening_linebreak.type = token::LINEBRK;
		opening_linebreak.val = "\n";
		opening_linebreak.pos += alias_token.val.size();

		Token end_namespace_token = alias_token;
		end_namespace_token.type = token::END_NAMESPACE;
		end_namespace_token.val = "end namespace";
		end_namespace_token.pos += alias_token.val.size();

		Token body_linebreak = end_namespace_token;
		body_linebreak.type = token::LINEBRK;
		body_linebreak.val = "\n";

		Token closing_linebreak = end_namespace_token;
		closing_linebreak.type = token::LINEBRK;
		closing_linebreak.val = "\n";
		closing_linebreak.pos += end_namespace_token.val.size();

		std::vector<std::unique_ptr<PreNodeAST>> namespaced_program;
		namespaced_program.reserve(program.chunk.size() + 6);
		namespaced_program.push_back(std::make_unique<PreNodeOther>(std::move(namespace_token), &program));
		namespaced_program.push_back(std::make_unique<PreNodeKeyword>(std::move(alias_token), &program));
		namespaced_program.push_back(std::make_unique<PreNodeOther>(std::move(opening_linebreak), &program));
		namespaced_program.insert(
			namespaced_program.end(),
			std::make_move_iterator(program.chunk.begin()),
			std::make_move_iterator(program.chunk.end())
		);
		namespaced_program.push_back(std::make_unique<PreNodeOther>(std::move(body_linebreak), &program));
		namespaced_program.push_back(std::make_unique<PreNodeOther>(std::move(end_namespace_token), &program));
		namespaced_program.push_back(std::make_unique<PreNodeOther>(std::move(closing_linebreak), &program));

		program.chunk = std::move(namespaced_program);
		// program.set_child_parents();
	}

	/// How an import spelled its alias, for a diagnostic that has to name both spellings.
	static std::string describe_alias(const std::string& alias) {
		return alias.empty() ? "without an alias" : "as <" + alias + ">";
	}

public:
	PreASTImport(SourceId  root_source,
	             SourceId  current_source,
	             SourceParser& parser,
	             std::unordered_map<std::string, std::string>& imported_files,
	             std::unordered_map<std::string, std::string>& basename_map,
	             ReferenceIndex* reference_index = nullptr)
		: PreASTVisitor(), m_current_source(std::move(current_source)), m_root_source(std::move(root_source)),
		  m_parser(parser), m_imported_files(imported_files), m_basename_map(basename_map),
		  m_reference_index(reference_index) {}

	PreNodeAST *visit(PreNodeImport &node) override {
		auto source_result = m_parser.resolve_import(m_root_source, m_current_source, node.path);
		if (source_result.is_error()) {
			auto error = source_result.get_error();
			error.set_token(node.tok);
			error.exit();
		}

		const auto import_source = source_result.unwrap();
		if (m_reference_index) {
			m_reference_index->add_file_link(
				node.path_token, import_source.value, node.path);
		}
		std::filesystem::path current_file_path(import_source.value);
		std::string import_path = current_file_path.string();
		const std::string alias = node.alias ? node.alias->tok.val : std::string();
		// check for circular dependencies
		const auto imported = m_imported_files.find(import_path);
		if (imported == m_imported_files.end()) {
			m_imported_files.emplace(import_path, alias);

			// check for basename conflicts
			auto basename = current_file_path.filename().string();
			auto it = m_basename_map.find(basename);
			if (it != m_basename_map.end() && it->second != import_path) {
				auto error = Diagnostic(ErrorType::CompileWarning, "", "", node.tok);
				error.message = "File with basename '" + basename + "' already imported from: " +
								  m_basename_map[basename] + ". \nImporting again from: " + import_path + ".";
				error.message += " This may lead to unexpected behavior.";
				// return Result<SuccessTag>(error);
				error.report(diagnostics());
			}
			m_basename_map[basename] = import_path;

			// parse
			auto program_result = m_parser.parse_pre_ast(import_source);
			if (program_result.is_error()) {
				program_result.get_error().exit();
			}
			auto import_program = std::move(program_result.unwrap());
			// remove end_token from imported programs
			import_program->program->chunk.pop_back();

			// recursively preprocess imports in the imported program to handle nested imports
			import_program->do_import_processing(
				m_root_source, import_source, m_parser, m_imported_files, m_basename_map,
				m_reference_index);

			for (auto& macro_def : import_program->macro_definitions) {
				macro_def->parent = m_program;
			}
			for (auto& define_stmt : import_program->define_statements) {
				define_stmt->parent = m_program;
			}
			m_program->define_statements.insert(m_program->define_statements.end(),
				std::make_move_iterator(import_program->define_statements.begin()),
				std::make_move_iterator(import_program->define_statements.end())
			);
			m_program->macro_definitions.insert(m_program->macro_definitions.end(),
				std::make_move_iterator(import_program->macro_definitions.begin()),
				std::make_move_iterator(import_program->macro_definitions.end())
			);
			wrap_imported_program_in_namespace(node, *import_program->program);
			node.replace_with(std::move(import_program->program));
		} else if (imported->second != alias) {
			// A file is imported once, so this import contributes nothing - but it was written
			// with a different alias than the one that won, and every name reached through it
			// would be missing. Silent for a plain second import, which is the ordinary case of
			// two files needing the same module.
			auto error = Diagnostic(ErrorType::FileError, "", "", node.tok);
			error.message = "<" + node.path + "> is already imported " + describe_alias(imported->second)
				+ " and cannot be imported again " + describe_alias(alias) + ".";
			error.add_message("A file is only imported once, so this import has no effect and its "
				"members stay reachable " + describe_alias(imported->second) + ". Import it once, or "
				"spell both imports the same way.");
			error.expected = "one import per file";
			error.exit();
		}

		return &node;

	}

	PreNodeAST *visit(PreNodeImportNCKP& node) override {
		auto source_result = m_parser.resolve_import(m_root_source, m_current_source, node.path);
		if (source_result.is_error()) {
			auto error = source_result.get_error();
			error.set_token(node.tok);
			error.exit();
		}

		const auto import_source = source_result.unwrap();
		if (m_reference_index) {
			m_reference_index->add_file_link(
				node.path_token, import_source.value, node.path);
		}
		auto json_result = m_parser.parse_json(import_source);
		if (json_result.is_error()) json_result.get_error().exit();
		auto json = std::move(json_result.unwrap());
		NCKPTranslator translator(m_program->def_provider);
		json->accept(translator);
		auto ui_variables = translator.collect_ui_variables();
		m_program->def_provider->set_external_variables(std::move(ui_variables));

		return node.replace_with(std::make_unique<PreNodeDeadCode>(node.tok, node.parent));
	}

	PreNodeAST *visit(PreNodeProgram &node) override {
		m_program = &node;
		visit_all(node.define_statements, *this);
		visit_all(node.macro_definitions, *this);
		node.program->accept(*this);
		return &node;
	}

	PreNodeAST* visit(PreNodeChunk &node) override {
		visit_all(node.chunk, *this);
		node.flatten();
		return &node;
	}


};
