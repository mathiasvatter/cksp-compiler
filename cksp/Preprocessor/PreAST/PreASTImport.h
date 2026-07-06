//
// Created by Mathias Vatter on 25.02.26.
//

#pragma once

#include "PreASTVisitor.h"
#include "../../Source/SourceParser.h"
#include "../../../JSON/NCKPTranslator.h"

class PreASTImport final : public PreASTVisitor {
	std::string m_debug_token;
	SourceId m_current_source;
	SourceId m_root_source;
	SourceParser& m_parser;
	std::unordered_set<std::string> &m_imported_files; // Um zirkuläre Abhängigkeiten zu vermeiden
	std::unordered_map<std::string, std::string> &m_basename_map; // Map to store basename to full path mapping

public:
	PreASTImport(const SourceId& root_source,
				 const SourceId& current_source,
				 SourceParser& parser,
	             std::unordered_set<std::string> &imported_files,
	             std::unordered_map<std::string, std::string> &basename_map)
		: PreASTVisitor(), m_current_source(current_source), m_root_source(root_source),
		  m_parser(parser), m_imported_files(imported_files), m_basename_map(basename_map) {}

	PreNodeAST *visit(PreNodeImport &node) override {
		auto source_result = m_parser.resolve_import(m_root_source, m_current_source, node.path);
		if (source_result.is_error()) {
			auto error = source_result.get_error();
			error.set_token(node.tok);
			error.exit();
		}

		const auto import_source = source_result.unwrap();
		std::filesystem::path current_file_path(import_source.value);
		std::string import_path = current_file_path.string();
		// check for circular dependencies
		if (!m_imported_files.contains(import_path)) {
			m_imported_files.insert(import_path);

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
				m_root_source, import_source, m_parser, m_imported_files, m_basename_map);

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
			node.replace_with(std::move(import_program->program));
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

		auto json_result = m_parser.parse_json(source_result.unwrap());
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
