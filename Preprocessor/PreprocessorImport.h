//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once
#include "Preprocessor.h"

class PreprocessorImport : public Preprocessor {
public:
    PreprocessorImport(std::vector<Token> tokens, std::string current_file);
private:
    std::vector<Token> m_imported_tokens;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;

    // Imports
    void process_imports(std::vector<Token> tokens, std::string current_file, std::unordered_set<std::string>& imported_files);
    Result<std::unique_ptr<NodeImport>> parse_import();
    Result<std::string> resolve_path(const std::string& import_path);
    std::string resolve_overlap(const std::string& base_path, const std::string& relative_path);
    void handle_imports();
};


