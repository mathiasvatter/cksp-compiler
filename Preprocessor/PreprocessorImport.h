//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once
#include "Preprocessor.h"

Result<std::string> resolve_path(const std::string& import_path, const Token& token, const std::string& curr_file);
static Result<std::string> resolve_overlap(const std::string& base_path, const std::string& relative_path);

class PreprocessorImport : public Preprocessor {
public:
    PreprocessorImport(std::vector<Token> tokens, std::string current_file,
                       const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets);

	Result<SuccessTag> process_imports();

private:
    std::vector<Token> m_imported_tokens;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;

    const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets;
//    std::vector<std::unique_ptr<NodeAST>> external_variables;

    // Imports
	Result<SuccessTag> process_import_statements(std::vector<Token>& tokens, const std::string& current_file);
    Result<std::unique_ptr<NodeImport>> parse_import(std::vector<Token>& tokens);
    Result<std::unique_ptr<NodeImport>> parse_import_nckp(std::vector<Token>& tokens);
	Result<SuccessTag> evaluate_import(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file);
    Result<SuccessTag> evaluate_import_nckp(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file);

};


