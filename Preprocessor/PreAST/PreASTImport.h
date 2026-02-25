//
// Created by Mathias Vatter on 25.02.26.
//

#pragma once

#include "PreASTVisitor.h"
#include "../../misc/PathHandler.h"
#include "../../Compiler.h"

class PreASTImport final : public PreASTVisitor {
	std::string m_debug_token;
	std::string m_current_file;

	std::unordered_set<std::string> m_imported_files; // Um zirkuläre Abhängigkeiten zu vermeiden
	std::unordered_map<std::string, std::string> m_basename_map; // Map to store basename to full path mapping

public:
	PreASTImport(const std::string &current_file,
	             const std::unordered_set<std::string> &imported_files,
	             const std::unordered_map<std::string, std::string> &basename_map)
		: PreASTVisitor(), m_current_file(current_file), m_imported_files(imported_files),
		  m_basename_map(basename_map) {
	}

	PreNodeAST *visit(PreNodeImport &node) override {
		auto root_directory = std::filesystem::path(m_current_file).parent_path().string();
		PathHandler path_handler(node.tok, m_current_file, root_directory);
		auto path = path_handler.resolve_import_path(node.path);
		if (path.is_error()) path.get_error().exit();

		std::filesystem::path current_file_path(path.unwrap());
		std::string import_path = current_file_path.string();
		// check for circular dependencies
		if (!m_imported_files.contains(import_path)) {
			m_imported_files.insert(import_path);

			// check for basename conflicts
			auto basename = current_file_path.filename().string();
			auto it = m_basename_map.find(basename);
			if (it != m_basename_map.end() && it->second != import_path) {
				auto error = CompileError(ErrorType::CompileWarning, "", "", node.tok);
				error.m_message = "File with basename '" + basename + "' already imported from: " +
								  m_basename_map[basename] + ". \nImporting again from: " + import_path + ".";
				error.m_message += " This may lead to unexpected behavior.";
				// return Result<SuccessTag>(error);
				error.print();
			}
			m_basename_map[basename] = import_path;

			// parse
			auto program_result = Compiler::preproc_parse(import_path, m_program->def_provider);
			if (program_result.is_error()) {
				program_result.get_error().exit();
			}
			auto import_program = std::move(program_result.unwrap());
			// remove end_token from imported programs
			import_program->program->chunk.pop_back();

			// recursively preprocess imports in the imported program to handle nested imports
			import_program->do_preprocessing(import_path, m_imported_files, m_basename_map);

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
		return &node;
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
