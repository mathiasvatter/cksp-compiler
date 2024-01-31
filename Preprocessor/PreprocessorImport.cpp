//
// Created by Mathias Vatter on 11.10.23.
//

#include "PreprocessorImport.h"
#include "../JSON/JSONParser.h"
#include "../JSON/JSONVisitor.h"
#include "../FileHandler.h"

PreprocessorImport::PreprocessorImport(std::vector<Token> tokens, std::string current_file,
               const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets)
        : Preprocessor(std::move(tokens), std::move(current_file)), m_builtin_widgets(m_builtin_widgets) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<SuccessTag> PreprocessorImport::process_imports() {
	m_imported_files.insert(m_current_file);
	auto result = process_import_statements(m_tokens, m_current_file);
	if(result.is_error())
		return Result<SuccessTag>(result.get_error());
	m_tokens.emplace_back(END_TOKEN, "", 0, 0,m_current_file);
	return Result<SuccessTag>(result.unwrap());
}


Result<SuccessTag> PreprocessorImport::process_import_statements(std::vector<Token>& tokens, const std::string& current_file) {
    m_pos = 0;
    while (peek(tokens).type != token::END_TOKEN) {
        if(peek(tokens).type == token::IMPORT) {
            auto import_stmt = parse_import(tokens);
            if (import_stmt.is_error())
                Result<SuccessTag>(import_stmt.get_error());
            else
                evaluate_import(tokens, import_stmt.unwrap(), current_file);
        } else if (peek(tokens).type == token::KEYWORD and peek(tokens).val == "import_nckp") {
            auto import_nckp_stmt = parse_import_nckp(tokens);
            if(import_nckp_stmt.is_error())
                Result<SuccessTag>(import_nckp_stmt.get_error());
            else
                evaluate_import_nckp(tokens, import_nckp_stmt.unwrap(), current_file);
        } else {
            consume(tokens);
        }
    }

	return Result<SuccessTag>(SuccessTag{});
}


Result<SuccessTag> PreprocessorImport::evaluate_import(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file) {
    auto alias = import_stmt->alias;
    auto path = resolve_path(import_stmt->filepath, peek(tokens), current_file);
    if(path.is_error()) {
        path.get_error().print();
    } else {
        if (m_imported_files.find(path.unwrap()) == m_imported_files.end()) {  // Überprüfe auf zirkuläre Abhängigkeiten
            m_imported_files.insert(path.unwrap());
//            std::cout << path.unwrap() << std::endl;
            FileHandler file_handler(path.unwrap());
            Tokenizer tokenizer(file_handler.get_output(), path.unwrap(), file_handler.get_file_type());
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
	return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorImport::evaluate_import_nckp(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file) {
    auto path = resolve_path(import_stmt->filepath, peek(tokens), current_file);
    if(path.is_error()) {
        path.get_error().print();
    } else {
//        std::cout << path.unwrap() << std::endl;
        FileHandler file_handler(path.unwrap());
        Tokenizer tokenizer(file_handler.get_output(), path.unwrap(), file_handler.get_file_type());
        auto nckp_tokens = tokenizer.tokenize();
        JSONParser parser(std::move(nckp_tokens));
        auto ast = parser.parse_json();
        NCKPTranslator translator(m_builtin_widgets);
        ast->accept(translator);
        auto ui_variables = translator.collect_ui_variables();
        m_external_variables = std::move(ui_variables);
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<std::unique_ptr<NodeImport>> PreprocessorImport::parse_import_nckp(std::vector<Token>& tokens) {
    size_t begin = m_pos;
    consume(tokens);
    if(peek(tokens).type != OPEN_PARENTH) {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Incorrect import_nckp Syntax.",peek(tokens).line,"(",peek(tokens).val, peek(tokens).file));
    }
    consume(tokens);
    if(peek(tokens).type !=STRING) {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::PreprocessorError,
        "Not a filepath",peek(tokens).line,"path",peek(tokens).val, peek(tokens).file));
    }
    std::string filepath = consume(tokens).val;
    // erase ""
    filepath.erase(0,1);
    filepath.pop_back();
    if(peek(tokens).type != CLOSED_PARENTH) {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Incorrect import_nckp Syntax.",peek(tokens).line,")",peek(tokens).val, peek(tokens).file));
    }
    consume(tokens);
    if(peek(tokens).type != token::LINEBRK)
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Incorrect import Syntax.",peek(tokens).line,"linebreak",peek(tokens).val, peek(tokens).file));
    consume(tokens); //consume linebreak
    remove_tokens(tokens, begin, m_pos);
    auto return_value = std::make_unique<NodeImport>(filepath, "", get_tok(tokens));
    return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
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
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::FileError,
    "Not a filepath",peek(tokens).line,"path",peek(tokens).val, peek(tokens).file));
    }
}

Result<std::string> resolve_path(const std::string& import_path, const Token& token, const std::string& curr_file) {
    std::filesystem::path current_file(curr_file);
    std::filesystem::path current_base = current_file.parent_path();
    std::filesystem::path rel(import_path);

    // Wenn der Pfad bereits absolut ist, überprüfe, ob er existiert
    if (rel.is_absolute()) {
        if (std::filesystem::exists(rel)) {
            return Result<std::string>(rel.string());
        } else {
            return Result<std::string>(CompileError(ErrorType::FileError,
			"Found incorrect path.", token.line, "valid path", rel.string(), curr_file));
        }
    }

    // Andernfalls mache den Pfad absolut in Bezug auf den Basispfad. Lexically normal removes ../
    std::filesystem::path combined = (current_base / rel).lexically_normal();
    std::filesystem::path absPath = std::filesystem::absolute(combined);

    // Überprüfe, ob der Pfad existiert
    if (std::filesystem::exists(absPath)) {
        return Result<std::string>(absPath.string());
    } else {
        auto resolve_error = current_base.string() + ", " + rel.string();
        auto new_absPath = resolve_overlap(current_base.string(), rel.string());
        if(new_absPath.is_error())
            return Result<std::string>(CompileError(ErrorType::FileError,
            "Could not resolve paths.", token.line, "valid path", resolve_error, curr_file));

        if (std::filesystem::exists(new_absPath.unwrap()))
            return Result<std::string>(new_absPath.unwrap());
        return Result<std::string>(CompileError(ErrorType::FileError,
		"Could not resolve paths.", token.line, "valid path", resolve_error, curr_file));
    }
}

Result<std::string> resolve_overlap(const std::string& base_path, const std::string& relative_path) {
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

    // Überprüfen, ob keine Übereinstimmung gefunden wurde
    if (baseParts.empty() || relativeParts.empty() || baseParts.back() != relativeParts.front()) {
        return Result<std::string>(CompileError(ErrorType::FileError,
        "Could not resolve paths.", -1, "valid path", "", ""));
    }

    baseParts.pop_back(); //erase common bit
    std::filesystem::path mergedPath;
    for (const auto& part : baseParts) {
        mergedPath /= part;
    }
    for (const auto& part : relativeParts) {
        mergedPath /= part;
    }
    return Result<std::string>(mergedPath.string());
}
