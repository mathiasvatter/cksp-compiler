//
// Created by Mathias Vatter on 11.10.23.
//

#include "PreprocessorImport.h"

PreprocessorImport::PreprocessorImport(std::vector<Token> tokens, std::string current_file)
        : Preprocessor(std::move(tokens), std::move(current_file)) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<SuccessTag> PreprocessorImport::process_imports() {
	m_imported_files.insert(m_current_file);
	auto result = process_import_statements(m_tokens, m_current_file);
	if(result.is_error())
		return Result<SuccessTag>(result.get_error());
	m_tokens.emplace_back(END_TOKEN, "", 0, m_current_file);
	return Result<SuccessTag>(result.unwrap());
}


Result<SuccessTag> PreprocessorImport::process_import_statements(std::vector<Token>& tokens, const std::string& current_file) {
//    m_tokens = std::move(m_tokens);
//    m_current_file = std::move(current_file);
    m_pos = 0;
    while (peek(tokens).type != token::END_TOKEN) {
        if(peek(tokens).type == token::IMPORT) {
            auto import_stmt = parse_import(tokens);
            if(import_stmt.is_error())
                Result<SuccessTag>(import_stmt.get_error());
            else
                evaluate_import(tokens, import_stmt.unwrap(), current_file);
//                m_import_statements.push_back(std::move(import_stmt.unwrap()));
        } else {
            consume(tokens);
        }
    }

	return Result<SuccessTag>(SuccessTag{});
}


Result<SuccessTag> PreprocessorImport::evaluate_import(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file) {
//    for (auto & import_stmt : m_import_statements) {
        auto alias = import_stmt->alias;
        auto path = resolve_path(import_stmt->filepath, tokens, current_file);
        if(path.is_error()) {
            path.get_error().print();
        } else {
            if (m_imported_files.find(path.unwrap()) == m_imported_files.end()) {  // Überprüfe auf zirkuläre Abhängigkeiten
                m_imported_files.insert(path.unwrap());
                std::cout << path.unwrap() << std::endl;
                Tokenizer tokenizer(path.unwrap());
                auto new_tokens = tokenizer.tokenize();

                size_t og_pos = m_pos;
                auto processed_imports = process_import_statements(new_tokens, path.unwrap());
				if(processed_imports.is_error())
					return Result<SuccessTag>(processed_imports.get_error());
                m_pos = og_pos;
                // check END_TOKEN status and remove END_TOKEN from imported files
                if (!tokens.empty() && tokens.back().type == token::END_TOKEN) {
                    tokens.pop_back();
                }
				tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());
            }
        }
//    }
	return Result<SuccessTag>(SuccessTag{});
}

Result<std::unique_ptr<NodeImport>> PreprocessorImport::parse_import(std::vector<Token>& tokens) {
    //consume import token IMPORT
    size_t begin = m_pos;
    consume(tokens);
    if(peek(tokens).type ==token::STRING) {
        std::string filepath = consume(tokens).val;
        // erase ""
        filepath.erase(0,1);
        filepath.pop_back();
        std::string alias;
        if(peek(tokens).type == token::AS) {
            // consume as token
            consume(tokens);
            if(peek(tokens).type == token::KEYWORD) {
                alias = consume(tokens).val;
            } else {
                return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
                                                                        "Incorrect import Syntax.",peek(tokens).line,"as <keyword>",peek(tokens).val, peek(tokens).file));
            }
        }
        if(peek(tokens).type != token::LINEBRK)
            return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
                                                                    "Incorrect import Syntax.",peek(tokens).line,"linebreak",peek(tokens).val, peek(tokens).file));
        consume(tokens); //consume linebreak
        remove_tokens(tokens, begin, m_pos);
        auto return_value = std::make_unique<NodeImport>(filepath, alias, get_tok(tokens));
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::PreprocessorError,
                                                                "Not a filepath",peek(tokens).line,"path",peek(tokens).val, peek(tokens).file));
    }
}

Result<std::string> PreprocessorImport::resolve_path(const std::string& import_path, std::vector<Token>& tokens, const std::string& curr_file) {
    std::filesystem::path current_file(curr_file);
    std::filesystem::path current_base = current_file.parent_path();
    std::filesystem::path rel(import_path);

    // Wenn der Pfad bereits absolut ist, überprüfe, ob er existiert
    if (rel.is_absolute()) {
        if (std::filesystem::exists(rel)) {
            return Result<std::string>(rel.string());
        } else {
            return Result<std::string>(CompileError(ErrorType::PreprocessorError,
			"Found incorrect path.", tokens.at(m_pos).line, "valid path", rel, curr_file));
        }
    }

    // Andernfalls mache den Pfad absolut in Bezug auf den Basispfad. Lexically normal removes ../
    std::filesystem::path combined = (current_base / rel).lexically_normal();
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
		"Could not resolve paths.", tokens.at(m_pos).line, "valid path", resolve_error, m_current_file));
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