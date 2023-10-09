//
// Created by Mathias Vatter on 23.09.23.
//

#include <filesystem>
#include <iostream>
#include "Preprocessor.h"

Preprocessor::Preprocessor(std::vector<Token> tokens, std::string current_file)
    : Parser(std::move(tokens)), m_current_file(std::move(current_file)) {
    m_imported_tokens = {};
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
    process_imports(m_tokens, m_current_file, m_imported_files);
    m_tokens = std::move(m_imported_tokens);
    m_tokens.emplace_back(END_TOKEN, "", 0, m_current_file);

    auto result = process_macros();
    if(result.is_error()) {
        result.get_error().print();
        auto err_msg = "Preprocessor failed while processing macros.";
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek().file).print();
        exit(EXIT_FAILURE);
    }
}

std::vector<Token> Preprocessor::get_tokens() {
    return std::move(m_tokens);
}

void Preprocessor::remove_last() {
    if (m_pos > 0 && m_pos <= m_tokens.size()) {
        m_tokens.erase(m_tokens.begin() + m_pos - 1);
        m_pos--;  // Adjust m_pos since we've removed an element before it
    } else {
        auto err_msg = "Attempted to remove a token out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, m_tokens.at(m_pos).line, "unknown", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
}

void Preprocessor::remove_tokens(size_t start, size_t end) {
    if (start < end && end <= m_tokens.size()) {
        m_tokens.erase(m_tokens.begin() + start, m_tokens.begin() + end);
        // Adjust m_pos if necessary
        if (m_pos > start)
            m_pos -= (end - start);
    } else {
        auto err_msg = "Attempted to remove a token range out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, m_tokens.at(m_pos).line, "unknown", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
}

void Preprocessor::process_imports(std::vector<Token> tokens, std::string current_file, std::unordered_set<std::string>& imported_files) {
    m_tokens = std::move(tokens);
    m_current_file = std::move(current_file);
    m_imported_files = imported_files;
    m_pos = 0;
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
    // check END_TOKEN status and remove END_TOKEN from imported files
    if (!m_tokens.empty() && m_tokens.back().type == token::END_TOKEN) {
        m_tokens.pop_back();
    }
    m_imported_tokens.insert(m_imported_tokens.end(), m_tokens.begin(), m_tokens.end());
    m_tokens.clear();
    handle_imports();
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
                process_imports(new_tokens, path.unwrap(), m_imported_files);
            }
        }
    }
}

Result<std::unique_ptr<NodeImport>> Preprocessor::parse_import() {
    //consume import token IMPORT
    size_t begin = m_pos;
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
        remove_tokens(begin, m_pos);
        auto return_value = std::make_unique<NodeImport>(filepath, alias, get_tok());
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::PreprocessorError,
        "Not a filepath",peek().line,"path",peek().val, peek().file));
    }
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

Result<SuccessTag> Preprocessor::process_macros() {
    m_pos = 0;
    // parse macro definitions
    while (peek().type != token::END_TOKEN) {
        if(peek().type == token::MACRO) {
            auto macro_definition = parse_macro_definition();
            if(macro_definition.is_error())
                return Result<SuccessTag>(macro_definition.get_error());
            else
                m_macro_definitions.push_back(std::move(macro_definition.unwrap()));
        } else {
            consume();
        }
    }
    m_pos = 0;
    while (peek().type != token::END_TOKEN) {
        if(is_macro_call()) {
			if (peek().type == LINEBRK) consume(); //consume linebreak
			if (is_defined_macro(peek().val)) {
				auto macro_call = parse_macro_call();
				if (macro_call.is_error())
					return Result<SuccessTag>(macro_call.get_error());
				m_macro_calls.push_back(std::move(macro_call.unwrap()));
			}
		} else if ((peek().type == token::LINEBRK and peek(1).type == ITERATE_MACRO) xor (m_pos == 0 and peek().type == ITERATE_MACRO)) {
			auto macro_iteration = parse_iterate_macro();
			if (macro_iteration.is_error())
				return Result<SuccessTag>(macro_iteration.get_error());
			m_macro_iterations.push_back(std::move(macro_iteration.unwrap()));
        } else {
            consume();
        }
    }
    return Result<SuccessTag>(SuccessTag{});
}

bool Preprocessor::is_macro_call() {
	bool macro_call = peek().type == token::LINEBRK && peek(1).type == token::KEYWORD && (peek(2).type == token::LINEBRK xor peek(2).type == token::OPEN_PARENTH);
	bool macro_call_beginning = m_pos == 0 && peek(0).type == token::KEYWORD && (peek(1).type == token::LINEBRK xor peek(1).type == token::OPEN_PARENTH);
	return macro_call xor macro_call_beginning;
}


bool Preprocessor::is_defined_macro(const std::string &name) {
    for(auto &macro_def : m_macro_definitions) {
        if(name == macro_def->header->name) {
            return true;
        }
    }
    return false;
}


Result<std::unique_ptr<NodeMacroHeader>> Preprocessor::parse_macro_header() {
    std::string macro_name;
    std::vector<std::vector<Token>> macro_args = {};
    macro_name = consume().val;
    if (peek().type == token::OPEN_PARENTH) {
        consume(); // consume (
        if (peek().type != token::CLOSED_PARENTH) {
            while (peek().type != token::CLOSED_PARENTH) {
                std::vector<Token> arg = {};
                while (peek().type != token::COMMA and peek().type != token::CLOSED_PARENTH) {
                    arg.push_back(consume());
                }
                if(peek().type == token::COMMA)
                    consume(); //consume COMMA
                macro_args.push_back(arg);
            }
        }
        consume();
    }
    auto value = std::make_unique<NodeMacroHeader>(macro_name, std::move(macro_args), get_tok());
    return Result<std::unique_ptr<NodeMacroHeader>>(std::move(value));
}

Result<std::unique_ptr<NodeMacroDefinition>> Preprocessor::parse_macro_definition() {
    std::unique_ptr<NodeMacroHeader> macro_header;
    std::vector<Token> macro_body;
    size_t begin = m_pos;
    consume(); //consume "macro"
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing macro name.",peek().line,"keyword",peek().val, peek().file));
    }
    auto header = parse_macro_header();
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(header.get_error());
    }
    macro_header = std::move(header.unwrap());
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing necessary linebreak after macro header.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak
    while (peek().type != token::END_MACRO) {
        if(peek().type == token::MACRO)
            return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
             "Nested macros are not allowed. Maybe you forgot an 'end macro' along the line?",peek().line,"linebreak",peek().val, peek().file));
        macro_body.push_back(consume());
    }
    consume(); // consume end macro
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
         "Missing necessary linebreak after 'end macro'.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); //consume linebreak
    remove_tokens(begin, m_pos);
	for(auto &arg : macro_header->args) {
		if(not(arg.size() == 1 and arg.at(0).type == token::KEYWORD))
			return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
			 "Only keywords allowed as parameters when defining macros.",arg.at(0).line,"keyword",arg.at(0).val, arg.at(0).file));
	}
    auto value = std::make_unique<NodeMacroDefinition>(std::move(macro_header), std::move(macro_body), get_tok());
    return Result<std::unique_ptr<NodeMacroDefinition>>(std::move(value));
}

Result<std::unique_ptr<NodeMacroHeader>> Preprocessor::parse_macro_call() {
    size_t begin = m_pos;
    auto macro_header = parse_macro_header();
    if(macro_header.is_error())
        return Result<std::unique_ptr<NodeMacroHeader>>(macro_header.get_error());
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroHeader>>(CompileError(ErrorType::SyntaxError,
         "Missing necessary linebreak after macro call. Maybe you forgot a closing parenthesis?",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak
    remove_tokens(begin, m_pos);
    return std::move(macro_header);
}

Result<std::unique_ptr<NodeIterateMacro>> Preprocessor::parse_iterate_macro() {
	size_t begin = m_pos;
	consume(); // consume iterate_macro
	if(peek().type != token::OPEN_PARENTH) {
		return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
		 "Found invalid iterate_macro statement syntax.",peek().line,"(",peek().val, peek().file));
	}
	consume(); //consume (
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid macro header in iterate_macro statement syntax.",peek().line,"valid <macro_header>",peek().val, peek().file));
	}
	Token macro_header = consume();
	if(peek().type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid iterate_macro statement syntax.",peek().line,")",peek().val, peek().file));
	}
	consume(); //consume )
	if(peek().type != token::ASSIGN) {
		return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
		  "Found invalid iterate_macro statement syntax.",peek().line,":=",peek().val, peek().file));
	}
	consume(); //consume :=
	auto from_stmt = parse_binary_expr();
	if(from_stmt.is_error())
		return Result<std::unique_ptr<NodeIterateMacro>>(from_stmt.get_error());
	SimpleInterpreter interpreter;
	auto from_interpreted = interpreter.evaluate_int_expression(from_stmt.unwrap());
	if(from_interpreted.is_error())
		return Result<std::unique_ptr<NodeIterateMacro>>(from_interpreted.get_error());
	int from = from_interpreted.unwrap();
	if(peek().type != token::TO) {
		return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
		  "Found invalid iterate_macro statement syntax.",peek().line,"to",peek().val, peek().file));
	}
	consume(); // consume "to"
	auto to_stmt = parse_binary_expr();
	if(to_stmt.is_error())
		return Result<std::unique_ptr<NodeIterateMacro>>(to_stmt.get_error());
	auto to_interpreted = interpreter.evaluate_int_expression(to_stmt.unwrap());
	if(to_interpreted.is_error())
		return Result<std::unique_ptr<NodeIterateMacro>>(to_interpreted.get_error());
	int to = to_interpreted.unwrap();
	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
		  "Found invalid iterate_macro statement syntax. Missing linebreak after iterate_macro statement.",peek().line,"linebreak",peek().val, peek().file));
	}
	consume(); //consume linebreak
	remove_tokens(begin, m_pos);

	auto result = std::make_unique<NodeIterateMacro>(std::move(macro_header), from, to);
	return Result<std::unique_ptr<NodeIterateMacro>>(std::move(result));
}


Result<int> SimpleInterpreter::evaluate_int_expression(std::unique_ptr<NodeAST>& root) {
	std::string preprocessor_error = "Statement needs to be evaluated to a single int value before compilation.";
	if (auto intNode = dynamic_cast<NodeInt*>(root.get())) {
		return Result<int>(intNode->value);
	} else if (auto unaryExprNode = dynamic_cast<NodeUnaryExpr*>(root.get())) {
		auto operandValueStmt = evaluate_int_expression(unaryExprNode->operand);
		if(operandValueStmt.is_error()) return Result<int>(operandValueStmt.get_error());
		int operandValue = operandValueStmt.unwrap();
		if (unaryExprNode->op.type == token::SUB) { // Assuming SUB represents the '-' unary operator
			return Result<int>(-operandValue);
		}
		// Add other unary operations here if needed
		return Result<int>(CompileError(ErrorType::PreprocessorError,
					 "Unsupported unary operation. "+preprocessor_error,
					 root->tok.line,"-",unaryExprNode->op.val, root->tok.file));
	} else if (auto binaryExprNode = dynamic_cast<NodeBinaryExpr*>(root.get())) {
		auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
		if(leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
		int leftValue = leftValueStmt.unwrap();
		auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
		if(rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
		int rightValue = rightValueStmt.unwrap();
		if (binaryExprNode->op == "+") {
			return Result<int>(leftValue + rightValue);
		} else if (binaryExprNode->op == "-") {
			return Result<int>(leftValue - rightValue);
		} else if (binaryExprNode->op == "*") {
			return Result<int>(leftValue * rightValue);
		} else if (binaryExprNode->op == "/") {
			if (rightValue == 0) {
				return Result<int>(CompileError(ErrorType::PreprocessorError,"Divison by zero. "+preprocessor_error,
				 root->tok.line,"", std::to_string(rightValue), root->tok.file));
			}
			return Result<int>(leftValue / rightValue);
		} else if (binaryExprNode->op == "mod") {
			return Result<int>(leftValue % rightValue);
		}
		// Add other binary operations here if needed
		return Result<int>(CompileError(ErrorType::PreprocessorError,"Unsupported binary operation. "+preprocessor_error,
		 root->tok.line,"*, /, -, +, mod", binaryExprNode->op, root->tok.file));
	}
	return Result<int>(CompileError(ErrorType::PreprocessorError,"Unknown expression statement. No variables allowed. "+preprocessor_error,
	 root->tok.line,"", "", root->tok.file));
}