//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once
#include "../Processor/Processor.h"
#include "../AST/ASTNodes/AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"
#include <unordered_set>


class ImportProcessor final : public Processor {
	DefinitionProvider* m_def_provider;
	std::string m_current_file;
	std::string m_root_directory;

    std::vector<Token> m_imported_tokens;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden
	std::unordered_map<std::string, std::string> m_basename_map; // Map to store basename to full path mapping
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;

    // Imports
	Result<SuccessTag> process_import_statements(std::vector<Token>& tokens, const std::string& current_file);
    Result<std::unique_ptr<NodeImport>> parse_import(std::vector<Token>& tokens);
    Result<std::unique_ptr<NodeImport>> parse_import_nckp(std::vector<Token>& tokens);
	Result<SuccessTag> evaluate_import(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file);
    Result<SuccessTag> evaluate_import_nckp(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file);

public:
    ImportProcessor(std::vector<Token> tokens, std::string current_file, DefinitionProvider* definition_provider);

	Result<SuccessTag> process_imports();


};


