//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once
#include "../Processor/Processor.h"
#include "../AST/ASTNodes/AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"
#include <unordered_set>

Result<std::string> resolve_path(const std::string& import_path, const Token& token, const std::string& curr_file);
static Result<std::string> resolve_overlap(const std::string& base_path, const std::string& relative_path);

class ImportProcessor : public Processor {
public:
    ImportProcessor(std::vector<Token> tokens, std::string current_file, DefinitionProvider* definition_provider);

	Result<SuccessTag> process_imports();
//	[[nodiscard]] const std::vector<std::unique_ptr<NodeDataStructure>> &get_external_variables() const;

private:
	std::string m_current_file;

    std::vector<Token> m_imported_tokens;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;

	DefinitionProvider* m_def_provider;
//    const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets;
//    std::vector<std::unique_ptr<NodeDataStructure>> m_external_variables;

    // Imports
	Result<SuccessTag> process_import_statements(std::vector<Token>& tokens, const std::string& current_file);
    Result<std::unique_ptr<NodeImport>> parse_import(std::vector<Token>& tokens);
    Result<std::unique_ptr<NodeImport>> parse_import_nckp(std::vector<Token>& tokens);
	Result<SuccessTag> evaluate_import(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file);
    Result<SuccessTag> evaluate_import_nckp(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file);

};


