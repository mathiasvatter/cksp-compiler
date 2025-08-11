//
// Created by Mathias Vatter on 11.10.23.
//

#include "ImportProcessor.h"

#include <utility>
#include <filesystem>
#include "../JSON/JSONParser.h"
#include "../JSON/JSONVisitor.h"
#include "../misc/FileHandler.h"
#include "../misc/PathHandler.h"

ImportProcessor::ImportProcessor(std::vector<Token> tokens, std::string current_file, DefinitionProvider* definition_provider)
        : Processor(std::move(tokens)), m_current_file(std::move(current_file)), m_def_provider(definition_provider) {
    m_pos = 0;
	m_curr_token_type = m_tokens.at(0).type;
    m_root_directory = std::filesystem::path(m_current_file).parent_path().string();
}

Result<SuccessTag> ImportProcessor::process_imports() {
	m_imported_files.insert(m_current_file);
	auto result = process_import_statements(m_tokens, m_current_file);
	if(result.is_error())
		return Result<SuccessTag>(result.get_error());
	m_tokens.emplace_back(token::END_TOKEN, "", 0, 0,m_current_file);
	return Result<SuccessTag>(result.unwrap());
}


Result<SuccessTag> ImportProcessor::process_import_statements(std::vector<Token>& tokens, const std::string& current_file) {
    m_pos = 0;
    while (peek(tokens).type != token::END_TOKEN) {
        if(peek(tokens).type == token::IMPORT) {
            auto import_stmt = parse_import(tokens);
            if (import_stmt.is_error())
                return Result<SuccessTag>(import_stmt.get_error());
            else {
                auto import_success = evaluate_import(tokens, import_stmt.unwrap(), current_file);
                if(import_success.is_error()) return Result<SuccessTag>(import_success.get_error());
            }
        } else if (peek(tokens).type == token::KEYWORD and peek(tokens).val == "import_nckp") {
            auto import_nckp_stmt = parse_import_nckp(tokens);
            if(import_nckp_stmt.is_error())
                return Result<SuccessTag>(import_nckp_stmt.get_error());
            else {
                auto import_success = evaluate_import_nckp(tokens, import_nckp_stmt.unwrap(), current_file);
                if(import_success.is_error()) return Result<SuccessTag>(import_success.get_error());
            }
        } else {
            consume(tokens);
        }
    }

	return Result<SuccessTag>(SuccessTag{});
}


Result<SuccessTag> ImportProcessor::evaluate_import(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file) {
    auto alias = import_stmt->alias;
	PathHandler path_handler(import_stmt->tok, current_file, m_root_directory);

    auto path = path_handler.resolve_import_path(import_stmt->filepath);
    if(path.is_error()) {
		return Result<SuccessTag>(path.get_error());
    }
    std::filesystem::path current_file_path(path.unwrap());
    std::string import_path = current_file_path.string();
    if (!m_imported_files.contains(import_path)) {  // Überprüfe auf zirkuläre Abhängigkeiten
        m_imported_files.insert(import_path);

        auto basename = current_file_path.filename().string();
        auto it = m_basename_map.find(basename);
        if (it != m_basename_map.end() && it->second != import_path) {
            auto error = CompileError(ErrorType::CompileWarning, "", "", import_stmt->tok);
            error.m_message = "File with basename '" + basename + "' already imported from: " +
                              m_basename_map[basename] + ". \nImporting again from: " + import_path + ".";
            error.m_message += " This may lead to unexpected behavior.";
            // return Result<SuccessTag>(error);
            error.print();
        }
        m_basename_map[basename] = import_path;

        FileHandler file_handler(import_path);
        Tokenizer tokenizer(file_handler.get_output(), import_path, file_handler.get_file_type());
        auto new_tokens = tokenizer.tokenize();

        size_t og_pos = m_pos;
        auto processed_imports = process_import_statements(new_tokens, import_path);
        if(processed_imports.is_error())
            return Result<SuccessTag>(processed_imports.get_error());
        m_pos = og_pos;
        // check END_TOKEN status and remove END_TOKEN from imported files
        if (!tokens.empty() && tokens.back().type == token::END_TOKEN) {
            tokens.pop_back();
        }
        tokens.insert(tokens.end(), new_tokens.begin(), new_tokens.end());
    }
	return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> ImportProcessor::evaluate_import_nckp(std::vector<Token>& tokens, std::unique_ptr<NodeImport>& import_stmt, const std::string& current_file) {
	PathHandler path_handler(peek(tokens), current_file, m_root_directory);
	// try to resolve current_file and import_path into absolute path
	auto path = path_handler.resolve_import_path(import_stmt->filepath);
    if(path.is_error()) {
		return Result<SuccessTag>(path.get_error());
	}
	FileHandler file_handler(path.unwrap());
	Tokenizer tokenizer(file_handler.get_output(), path.unwrap(), file_handler.get_file_type());
	auto nckp_tokens = tokenizer.tokenize();
	JSONParser parser(std::move(nckp_tokens));
	auto ast = parser.parse_json();
	NCKPTranslator translator(m_def_provider);
	ast->accept(translator);
	auto ui_variables = translator.collect_ui_variables();
	m_def_provider->set_external_variables(std::move(ui_variables));
    return Result<SuccessTag>(SuccessTag{});
}

Result<std::unique_ptr<NodeImport>> ImportProcessor::parse_import_nckp(std::vector<Token>& tokens) {
    size_t begin = m_pos;
    consume(tokens);
    if(peek(tokens).type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Incorrect import_nckp Syntax.",peek(tokens).line,"(",peek(tokens).val, peek(tokens).file));
    }
    consume(tokens);
    if(peek(tokens).type != token::STRING) {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::PreprocessorError,
        "Not a filepath",peek(tokens).line,"path",peek(tokens).val, peek(tokens).file));
    }
    Token path = consume(tokens);
    std::string filepath = StringUtils::remove_quotes(path.val);
    if(peek(tokens).type != token::CLOSED_PARENTH) {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Incorrect import_nckp Syntax.",peek(tokens).line,")",peek(tokens).val, peek(tokens).file));
    }
    consume(tokens);
    if(peek(tokens).type != token::LINEBRK)
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Incorrect import Syntax.",peek(tokens).line,"linebreak",peek(tokens).val, peek(tokens).file));
    consume(tokens); //consume linebreak
    remove_tokens(tokens, begin, m_pos);
    auto return_value = std::make_unique<NodeImport>(filepath, "", path);
    return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
}

Result<std::unique_ptr<NodeImport>> ImportProcessor::parse_import(std::vector<Token>& tokens) {
    //consume import token IMPORT
    size_t begin = m_pos;
    consume(tokens);
    if(peek(tokens).type ==token::STRING) {
        Token path = consume(tokens);
        std::string filepath = StringUtils::remove_quotes(path.val);
        std::string alias;
        if(peek(tokens).type == token::AS) {
            auto error = CompileError(ErrorType::ParseError, "", "", peek(tokens));
            error.set_message("Importing file with alias is not supported in as of version " + COMPILER_VERSION + ". "+
                              "Please remove the 'as <alias>' part from the import statement.");
            error.exit();
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
        auto return_value = std::make_unique<NodeImport>(filepath, alias, path);
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::FileError,
    "Not a filepath",peek(tokens).line,"path",peek(tokens).val, peek(tokens).file));
    }
}

