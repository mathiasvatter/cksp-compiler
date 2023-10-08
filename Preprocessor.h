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
	~Preprocessor() = default;
    std::vector<Token> get_tokens();
private:
    size_t m_pos;
    std::vector<Token> m_tokens;
    std::vector<Token> m_imported_tokens;
    std::string m_current_file;
    token m_curr_token;
    std::unordered_set<std::string> m_imported_files;  // Um zirkuläre Abhängigkeiten zu vermeiden
    std::vector<std::unique_ptr<NodeImport>> m_import_statements;
    std::vector<std::unique_ptr<NodeMacroDefinition>> m_macro_definitions;
    std::vector<std::unique_ptr<NodeMacroHeader>> m_macro_calls;

    Token& get_tok();
    Token peek(int ahead=0);
    Token consume();
    void remove_last();
    void remove_tokens(size_t start, size_t end);

    // Imports
	void process_imports(std::vector<Token> tokens, std::string current_file, std::unordered_set<std::string>& imported_files);
    Result<std::unique_ptr<NodeImport>> parse_import();
    Result<std::string> resolve_path(const std::string& import_path);
    std::string resolve_overlap(const std::string& base_path, const std::string& relative_path);
    void handle_imports();

    // Macros
    Result<SuccessTag> process_macros();
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_header();
    Result<std::unique_ptr<NodeMacroDefinition>> parse_macro_definition();
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_call();
    bool is_defined_macro(const std::string &name);
};

