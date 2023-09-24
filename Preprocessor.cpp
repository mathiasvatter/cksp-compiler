//
// Created by Mathias Vatter on 23.09.23.
//

#include <filesystem>
#include <iostream>
#include "Preprocessor.h"

Preprocessor::Preprocessor(std::vector<Token> tokens, std::string current_file)
    : m_tokens(std::move(tokens)), m_current_file(std::move(current_file)) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;

    while (peek().type != token::END_TOKEN) {
        if(peek().type == token::IMPORT) {
            auto import_stmt = parse_import();
            if(import_stmt.is_error())
                import_stmt.get_error().print();
            else
                m_import_statements.push_back(std::move(import_stmt.unwrap()));
        } else {
            consume();
        }
    }
    handle_imports();
}

Token Preprocessor::peek(int ahead) {
    if (m_tokens.size() < m_pos+ahead) {
        auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
        CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val).print();
        exit(EXIT_FAILURE);
    }
    m_curr_token = m_tokens.at(m_pos+ahead).type;
    return m_tokens.at(m_pos+ahead);
}

Token Preprocessor::consume() {
    if (m_pos < m_tokens.size()) {
        m_curr_token = m_tokens.at(m_pos + 1).type;
        return m_tokens.at(m_pos++);
    }
    auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
    CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val).print();
    exit(EXIT_FAILURE);
}

Result<std::unique_ptr<NodeImport>> Preprocessor::parse_import() {
    //consume import token IMPORT
    consume();
    if(peek().type ==token::STRING) {
        std::string filepath = consume().val;
        // erase ""
        filepath.erase(0,1);
        filepath.pop_back();
        std::string alias;
        if(peek().type == token::AS) {
            // consume as token
            consume();
            if(peek().type == token::KEYWORD) {
                alias = consume().val;
            } else {
                return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
                "Incorrect import Syntax.",peek().line,"as <keyword>",peek().val));
            }
        }
        auto return_value = std::make_unique<NodeImport>(filepath, alias);
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Not a filepath",peek().line,"path",peek().val));
    }
}

void Preprocessor::handle_imports() {
    for (auto & import_stmt : m_import_statements) {
        auto alias = import_stmt->alias;
        auto path = resolve_path(import_stmt->filepath);
        if(path.is_error()) {
            path.get_error().print();
        } else {
            if (m_imported_files.find(path.unwrap()) == m_imported_files.end()) {  // Überprüfe auf zirkuläre Abhängigkeiten
                m_imported_files.insert(path.unwrap());

                Tokenizer tokenizer(path.unwrap());
                auto new_tokens = tokenizer.tokenize();

                Preprocessor preprocessor(new_tokens, path.unwrap());  // Rekursiver Aufruf

                // Hier die Tokens der importierten Datei in m_tokens einfügen
                auto tokens = preprocessor.get_tokens();
                m_tokens.insert(m_tokens.end(), tokens.begin(), tokens.end());
            }
        }
    }
}

std::vector<Token> Preprocessor::get_tokens() {
    return std::move(m_tokens);
}

Result<std::string> Preprocessor::resolve_path(const std::string& import_path) {
    std::filesystem::path current_file(m_current_file);
    std::filesystem::path current_base = current_file.parent_path();
    std::filesystem::path rel(import_path);

    // Wenn der Pfad bereits absolut ist, überprüfe, ob er existiert
    if (rel.is_absolute()) {
        if (std::filesystem::exists(rel)) {
            return Result<std::string>(rel.string());
        } else {
            return Result<std::string>(CompileError(ErrorType::ParseError,
            "Found incorrect path.", 0, "valid path", rel));
        }
    }

    // Andernfalls mache den Pfad absolut in Bezug auf den Basispfad
    std::filesystem::path combined = current_base / rel;
    std::filesystem::path absPath = std::filesystem::absolute(combined);

    // Überprüfe, ob der Pfad existiert
    if (std::filesystem::exists(absPath)) {
        return Result<std::string>(absPath.string());
    } else {
        auto new_absPath = resolve_overlap(current_base, rel);
        if (std::filesystem::exists(new_absPath))
            return Result<std::string>(new_absPath);
        return Result<std::string>(CompileError(ErrorType::ParseError,
        "Found incorrect path.", 0, "valid path", new_absPath));
    }
}

std::string Preprocessor::resolve_overlap(const std::string& base_path, const std::string& relative_path) {
    std::filesystem::path base(base_path);
    std::filesystem::path rel(relative_path);

    std::vector<std::string> baseComponents;
    for (const auto& part : base) {
        baseComponents.push_back(part.string());
    }

    std::vector<std::string> relComponents;
    for (const auto& part : rel) {
        relComponents.push_back(part.string());
    }

    // Finde den längsten gemeinsamen Suffix
    auto itBase = baseComponents.rbegin();
    auto itRel = relComponents.rbegin();
    std::vector<std::string> commonSuffix;

    while (itBase != baseComponents.rend() && itRel != relComponents.rend()) {
        if (*itBase != *itRel) {
            break;
        }
        commonSuffix.push_back(*itBase);
        ++itBase;
        ++itRel;
    }

    // Entferne den gemeinsamen Suffix vom Basispfad und vom relativen Pfad
    baseComponents.erase(itBase.base(), baseComponents.end());
    relComponents.erase(relComponents.begin(), itRel.base());

    // Kombiniere die Pfade
    std::filesystem::path result;
    for (const auto& part : baseComponents) {
        result /= part;
    }
    for (const auto& part : relComponents) {
        result /= part;
    }

    return result.string();
}