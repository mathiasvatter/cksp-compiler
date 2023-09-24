//
// Created by Mathias Vatter on 23.09.23.
//

#pragma once
#include <unordered_set>

#include "Tokenizer.h"
#include "AST.h"

class Preprocessor {
public:
    Preprocessor(std::vector<Token> tokens, std::string current_file);

    std::vector<Token> get_tokens();
private:
    size_t m_pos;
    std::vector<Token> m_tokens;
    std::string m_current_file;
    token m_curr_token;
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden

    Token peek(int ahead=0);
    Token consume();

    Result<std::unique_ptr<NodeImport>> parse_import();
    Result<std::string> resolve_path(const std::string& import_path);
    std::string resolve_overlap(const std::string& base_path, const std::string& relative_path);
    void handle_imports();
};

