//
// Created by Mathias Vatter on 23.09.23.
//

#include <filesystem>
#include <iostream>
#include "Preprocessor.h"

Preprocessor::Preprocessor(std::vector<Token> tokens, std::string current_file, std::unordered_set<std::string>& imported_files)
    : m_tokens(std::move(tokens)), m_current_file(std::move(current_file)), m_imported_files(imported_files) {
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
	main_loop();
}

void Preprocessor::main_loop() {
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
	// check END_TOKEN status
	if (!m_tokens.empty() && m_tokens.back().type == token::END_TOKEN) {
		m_tokens.pop_back();
	}
    handle_imports();
	m_tokens.emplace_back(END_TOKEN, "", 0, m_current_file);
}

Token& Preprocessor::get_tok() {
    return m_tokens.at(m_pos);
}

Token Preprocessor::peek(int ahead) {
    if (m_tokens.size() < m_pos+ahead) {
        auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
        CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
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
    CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
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
                "Incorrect import Syntax.",peek().line,"as <keyword>",peek().val, peek().file));
            }
        }
        if(peek().type != token::LINEBRK)
            return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
            "Incorrect import Syntax.",peek().line,"linebreak",peek().val, peek().file));
        consume(); //consume linebreak
        auto return_value = std::make_unique<NodeImport>(filepath, alias, get_tok());
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Not a filepath",peek().line,"path",peek().val, peek().file));
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
                std::cout << path.unwrap() << std::endl;
                Tokenizer tokenizer(path.unwrap());
                auto new_tokens = tokenizer.tokenize();
                Preprocessor preprocessor(new_tokens, path.unwrap(), m_imported_files);  // Rekursiver Aufruf

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
            "Found incorrect path.", m_tokens.at(m_pos).line, "valid path", rel, m_current_file));
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
        auto resolve_error = current_base.string() + ", " + rel.string();
        return Result<std::string>(CompileError(ErrorType::ParseError,
        "Could not resolve paths.", m_tokens.at(m_pos).line, "valid path", resolve_error, m_current_file));
    }
}

std::string Preprocessor::resolve_overlap(const std::string& base_path, const std::string& relative_path) {
    std::filesystem::path base(base_path);
    std::filesystem::path relative(relative_path);

    std::vector<std::filesystem::path> baseParts;
    for (const auto& part : base) {
        baseParts.push_back(part);
    }

    std::vector<std::filesystem::path> relativeParts;
    for (const auto& part : relative) {
        relativeParts.push_back(part);
    }
    // do as long as its
    while (!baseParts.empty() && !relativeParts.empty() && baseParts.back() != relativeParts.front()) {
        baseParts.pop_back();
    }
    baseParts.pop_back(); //erase common bit

    std::filesystem::path mergedPath;
    for (const auto& part : baseParts) {
        mergedPath /= part;
    }
    for (const auto& part : relativeParts) {
        mergedPath /= part;
    }

    return mergedPath.string();
}
