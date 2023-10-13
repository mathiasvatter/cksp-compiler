//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once
#include "Preprocessor.h"

class PreprocessorImport : public Preprocessor {
public:
    PreprocessorImport(std::vector<Token> tokens, std::string current_file);
	Result<SuccessTag> process_imports();
private:
    std::vector<Token> m_imported_tokens;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;

    // Imports
	Result<SuccessTag> process_import_statements(std::vector<Token>& tokens, std::string current_file);
    Result<std::unique_ptr<NodeImport>> parse_import(std::vector<Token>& tokens);
    Result<std::string> resolve_path(const std::string& import_path, std::vector<Token>& tokens);
    std::string resolve_overlap(const std::string& base_path, const std::string& relative_path);
	Result<SuccessTag> evaluate_imports(std::vector<Token>& tokens);
};


