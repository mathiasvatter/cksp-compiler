//
// Created by Mathias Vatter on 11.10.23.
//

#include "PreprocessorImport.h"

PreprocessorImport::PreprocessorImport(std::vector<Token> tokens, std::string current_file)
        : Preprocessor(std::move(tokens), std::move(current_file)) {
    m_imported_tokens = {};
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
    process_imports(m_tokens, m_current_file, m_imported_files);
    m_tokens = std::move(m_imported_tokens);
    m_tokens.emplace_back(END_TOKEN, "", 0, m_current_file);
}

void PreprocessorImport::process_imports(std::vector<Token> tokens, std::string current_file, std::unordered_set<std::string>& imported_files) {
    m_tokens = std::move(tokens);
    m_current_file = std::move(current_file);
    m_imported_files = imported_files;
    m_pos = 0;
    while (peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::IMPORT) {
            auto import_stmt = parse_import();
            if(import_stmt.is_error())
                import_stmt.get_error().print();
            else
                m_import_statements.push_back(std::move(import_stmt.unwrap()));
        } else {
            consume(m_tokens);
        }
    }
    // check END_TOKEN status and remove END_TOKEN from imported files
    if (!m_tokens.empty() && m_tokens.back().type == token::END_TOKEN) {
        m_tokens.pop_back();
    }
    m_imported_tokens.insert(m_imported_tokens.end(), m_tokens.begin(), m_tokens.end());
    m_tokens.clear();
    handle_imports();
}


void PreprocessorImport::handle_imports() {
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
                process_imports(new_tokens, path.unwrap(), m_imported_files);
            }
        }
    }
}

Result<std::unique_ptr<NodeImport>> PreprocessorImport::parse_import() {
    //consume import token IMPORT
    size_t begin = m_pos;
    consume(m_tokens);
    if(peek(m_tokens).type ==token::STRING) {
        std::string filepath = consume(m_tokens).val;
        // erase ""
        filepath.erase(0,1);
        filepath.pop_back();
        std::string alias;
        if(peek(m_tokens).type == token::AS) {
            // consume as token
            consume(m_tokens);
            if(peek(m_tokens).type == token::KEYWORD) {
                alias = consume(m_tokens).val;
            } else {
                return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
                                                                        "Incorrect import Syntax.",peek(m_tokens).line,"as <keyword>",peek(m_tokens).val, peek(m_tokens).file));
            }
        }
        if(peek(m_tokens).type != token::LINEBRK)
            return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
                                                                    "Incorrect import Syntax.",peek(m_tokens).line,"linebreak",peek(m_tokens).val, peek(m_tokens).file));
        consume(m_tokens); //consume linebreak
        remove_tokens(m_tokens, begin, m_pos);
        auto return_value = std::make_unique<NodeImport>(filepath, alias, get_tok(m_tokens));
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::PreprocessorError,
                                                                "Not a filepath",peek(m_tokens).line,"path",peek(m_tokens).val, peek(m_tokens).file));
    }
}

Result<std::string> PreprocessorImport::resolve_path(const std::string& import_path) {
    std::filesystem::path current_file(m_current_file);
    std::filesystem::path current_base = current_file.parent_path();
    std::filesystem::path rel(import_path);

    // Wenn der Pfad bereits absolut ist, überprüfe, ob er existiert
    if (rel.is_absolute()) {
        if (std::filesystem::exists(rel)) {
            return Result<std::string>(rel.string());
        } else {
            return Result<std::string>(CompileError(ErrorType::PreprocessorError,
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
        return Result<std::string>(CompileError(ErrorType::PreprocessorError,
                                                "Could not resolve paths.", m_tokens.at(m_pos).line, "valid path", resolve_error, m_current_file));
    }
}

std::string PreprocessorImport::resolve_overlap(const std::string& base_path, const std::string& relative_path) {
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