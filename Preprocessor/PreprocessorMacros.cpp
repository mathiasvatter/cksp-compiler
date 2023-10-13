//
// Created by Mathias Vatter on 11.10.23.
//

#include "PreprocessorMacros.h"

PreprocessorMacros::PreprocessorMacros(std::vector<Token> tokens, const std::string &currentFile)
	: Preprocessor(std::move(tokens),currentFile) {
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
}


Result<SuccessTag> PreprocessorMacros::process_macros() {
    auto processed_macro_defs = process_macro_definitions();
    if(processed_macro_defs.is_error())
        return Result<SuccessTag>(processed_macro_defs.get_error());

    auto processed_macro_calls = process_macro_calls(m_tokens);
    if(processed_macro_calls.is_error())
        return Result<SuccessTag>(processed_macro_calls.get_error());
    return Result<SuccessTag>(processed_macro_calls.unwrap());
}

Result<SuccessTag> PreprocessorMacros::process_macro_definitions() {
    m_pos = 0;
    // parse macro definitions
    while (peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::MACRO) {
            auto macro_definition = parse_macro_definition(m_tokens);
            if(macro_definition.is_error())
                return Result<SuccessTag>(macro_definition.get_error());
            else
                m_macro_definitions.push_back(std::move(macro_definition.unwrap()));
        } else {
            consume(m_tokens);
        }
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorMacros::process_macro_calls(std::vector<Token>& tok) {
    m_pos = 0;
    while (peek(tok).type != token::END_TOKEN) {
        if(is_macro_call(tok)) {
            if (is_defined_macro(peek(tok).val)) {
                auto macro_call = parse_macro_call(tok);
                if (macro_call.is_error())
                    return Result<SuccessTag>(macro_call.get_error());
                auto macro_call_evaluation = evaluate_macro_call(std::move(macro_call.unwrap()), tok);
				if (macro_call_evaluation.is_error())
					return Result<SuccessTag>(macro_call_evaluation.get_error());
            } else consume(tok);
        } else if ((peek(tok).type == token::LINEBRK and peek(tok, 1).type == ITERATE_MACRO) xor (m_pos == 0 and peek(tok).type == ITERATE_MACRO)) {
            if (peek(tok).type == LINEBRK) consume(tok); //consume linebreak
            auto macro_iteration = parse_iterate_macro(tok);
            if (macro_iteration.is_error())
                return Result<SuccessTag>(macro_iteration.get_error());
            m_macro_iterations.push_back(std::move(macro_iteration.unwrap()));
        } else consume(tok);
    }
    return Result<SuccessTag>(SuccessTag{});
}


Result<SuccessTag> PreprocessorMacros::evaluate_macro_call(std::unique_ptr<NodeMacroHeader> macro_call, std::vector<Token>& tok) {
    std::vector<Token> macro_body;
    std::vector<std::vector<Token>> macro_params = {};
    bool valid_macro_call = false;
    for(auto &macro_def : m_macro_definitions) {
        if(macro_call->name == macro_def->header->name and macro_call->args.size() == macro_def->header->args.size()) {
            macro_body = macro_def->body;
            macro_params = macro_def->header->args;
            valid_macro_call = true;
            break;
        }
    }

    // return false if no macro definition found has the necessary amount of params of called macro
    if(!valid_macro_call)
        return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	   "Found incorrect number of parameters in macro call.", macro_call->tok.line, "valid number of parameters", std::to_string(macro_call->args.size()), macro_call->tok.file));

    // check if called macro is already being evaluated at the moment -> recursive macro call
    if(std::find(m_macro_evaluation_stack.begin(), m_macro_evaluation_stack.end(), macro_call->name) != m_macro_evaluation_stack.end()) {
        // recursive macro call detected
        return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	   "Recursive macro call detected. Calling the macro inside its definition is not allowed.", macro_call->tok.line, "", macro_call->name, macro_call->tok.file));
    }
    m_macro_evaluation_stack.push_back(macro_call->name);

    // substitution
    for(int i = 0; i < macro_params.size(); i++) {
        for (size_t j = 0; j < macro_body.size(); ) {
            // find keyword from params in body
            if(macro_params[i][0].val == macro_body[j].val) {
                // Füge die Tokens aus macro_call->args[i] vor dem aktuellen Token ein
                macro_body.insert(macro_body.begin() + j, macro_call->args[i].begin(), macro_call->args[i].end());
                // Lösche das aktuelle Token
                macro_body.erase(macro_body.begin() + j + macro_call->args[i].size());
            } else {
                ++j;
            }
        }
    }
	// add end_token so that process_macro_calls while loop stops
	macro_body.emplace_back(END_TOKEN, "", 0, m_current_file);

    auto macro_call_result = process_macro_calls(macro_body);
    if (macro_call_result.is_error())
        return Result<SuccessTag>(macro_call_result.get_error());

	if(macro_body.back().type == token::END_TOKEN) macro_body.pop_back(); // delete end_token
	// alter m_pos to the position of the start of the macro_call (since all macro_call tokens got deleted in parse_macro_call)
    m_pos = macro_call->token_pos;
	// insert macro_body into tok at position of macro_call
    tok.insert(tok.begin() + m_pos, macro_body.begin(), macro_body.end());
    m_pos += macro_body.size();
	// delete current macro name from evaluation stack
    m_macro_evaluation_stack.pop_back();
    return Result<SuccessTag>(SuccessTag{});
}


bool PreprocessorMacros::is_macro_call(const std::vector<Token>& tok) {
	if(m_pos > 0)
		return peek(tok, -1).type == LINEBRK && peek(tok, 0).type == token::KEYWORD && (peek(tok, 1).type == token::LINEBRK xor peek(tok, 1).type == token::OPEN_PARENTH);
	else
		return m_pos == 0 && peek(tok, 0).type == token::KEYWORD && (peek(tok, 1).type == token::LINEBRK xor peek(tok, 1).type == token::OPEN_PARENTH);
}


bool PreprocessorMacros::is_defined_macro(const std::string &name) {
    for(auto &macro_def : m_macro_definitions) {
        if(name == macro_def->header->name) {
            return true;
        }
    }
    return false;
}


Result<std::unique_ptr<NodeMacroHeader>> PreprocessorMacros::parse_macro_header(std::vector<Token>& tok) {
    std::string macro_name;
    std::vector<std::vector<Token>> macro_args = {};
	size_t start_pos = m_pos;
    macro_name = consume(tok).val;
    if (peek(tok).type == token::OPEN_PARENTH) {
        consume(tok); // consume (
        if (peek(tok).type != token::CLOSED_PARENTH) {
            while (peek(tok).type != token::CLOSED_PARENTH) {
                std::vector<Token> arg = {};
                while (peek(tok).type != token::COMMA and peek(tok).type != token::CLOSED_PARENTH) {
                    arg.push_back(consume(tok));
                }
                if(peek(tok).type == token::COMMA)
                    consume(tok); //consume COMMA
                macro_args.push_back(arg);
            }
        }
        consume(tok);
    }
    auto value = std::make_unique<NodeMacroHeader>(macro_name, std::move(macro_args), start_pos, get_tok(tok));
    return Result<std::unique_ptr<NodeMacroHeader>>(std::move(value));
}

Result<std::unique_ptr<NodeMacroDefinition>> PreprocessorMacros::parse_macro_definition(std::vector<Token>& tok) {
    std::unique_ptr<NodeMacroHeader> macro_header;
    std::vector<Token> macro_body;
    size_t begin = m_pos;
    consume(tok); //consume "macro"
    if (peek(tok).type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
                                                                         "Missing macro name.",peek(tok).line,"keyword",peek(tok).val, peek(tok).file));
    }
    auto header = parse_macro_header(tok);
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(header.get_error());
    }
    macro_header = std::move(header.unwrap());
    if (peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
                                                                         "Missing necessary linebreak after macro header.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume linebreak
    while (peek(tok).type != token::END_MACRO) {
        if(peek(tok).type == token::MACRO)
            return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
                                                                             "Nested macros are not allowed. Maybe you forgot an 'end macro' along the line?",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
        macro_body.push_back(consume(tok));
    }
    consume(tok); // consume end macro
    if (peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
                                                                         "Missing necessary linebreak after 'end macro'.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume linebreak
    remove_tokens(tok, begin, m_pos);
    for(auto &arg : macro_header->args) {
        if(not(arg.size() == 1 and arg.at(0).type == token::KEYWORD))
            return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
                                                                             "Only keywords allowed as parameters when defining macros.",arg.at(0).line,"keyword",arg.at(0).val, arg.at(0).file));
    }
    auto value = std::make_unique<NodeMacroDefinition>(std::move(macro_header), std::move(macro_body), get_tok(tok));
    return Result<std::unique_ptr<NodeMacroDefinition>>(std::move(value));
}

Result<std::unique_ptr<NodeMacroHeader>> PreprocessorMacros::parse_macro_call(std::vector<Token>& tok) {
    size_t begin = m_pos;
    auto macro_header = parse_macro_header(tok);
    if(macro_header.is_error())
        return Result<std::unique_ptr<NodeMacroHeader>>(macro_header.get_error());
    if (peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroHeader>>(CompileError(ErrorType::SyntaxError,
                                                                     "Missing necessary linebreak after macro call. Maybe you forgot a closing parenthesis?",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume linebreak
    remove_tokens(tok, begin, m_pos);
    return std::move(macro_header);
}

Result<std::unique_ptr<NodeIterateMacro>> PreprocessorMacros::parse_iterate_macro(std::vector<Token>& tok) {
    size_t begin = m_pos;
    consume(tok); // consume iterate_macro
    if(peek(tok).type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
                                                                      "Found invalid iterate_macro statement syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume (
    if(peek(tok).type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
                                                                      "Found invalid macro header in iterate_macro statement syntax.",peek(tok).line,"valid <macro_header>",peek(tok).val, peek(tok).file));
    }
    Token macro_header = consume(tok);
    if(peek(tok).type != token::CLOSED_PARENTH) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
                                                                      "Found invalid iterate_macro statement syntax.",peek(tok).line,")",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume )
    if(peek(tok).type != token::ASSIGN) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
                                                                      "Found invalid iterate_macro statement syntax.",peek(tok).line,":=",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume :=
    auto from_stmt = parse_binary_expr();
    if(from_stmt.is_error())
        return Result<std::unique_ptr<NodeIterateMacro>>(from_stmt.get_error());
    SimpleInterpreter interpreter;
    auto from_interpreted = interpreter.evaluate_int_expression(from_stmt.unwrap());
    if(from_interpreted.is_error())
        return Result<std::unique_ptr<NodeIterateMacro>>(from_interpreted.get_error());
    int from = from_interpreted.unwrap();
    if(peek(tok).type != token::TO) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
                                                                      "Found invalid iterate_macro statement syntax.",peek(tok).line,"to",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume "to"
    auto to_stmt = parse_binary_expr();
    if(to_stmt.is_error())
        return Result<std::unique_ptr<NodeIterateMacro>>(to_stmt.get_error());
    auto to_interpreted = interpreter.evaluate_int_expression(to_stmt.unwrap());
    if(to_interpreted.is_error())
        return Result<std::unique_ptr<NodeIterateMacro>>(to_interpreted.get_error());
    int to = to_interpreted.unwrap();
    if(peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
                                                                      "Found invalid iterate_macro statement syntax. Missing linebreak after iterate_macro statement.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume linebreak
    remove_tokens(tok, begin, m_pos);

    auto result = std::make_unique<NodeIterateMacro>(std::move(macro_header), from, to);
    return Result<std::unique_ptr<NodeIterateMacro>>(std::move(result));
}


Result<int> SimpleInterpreter::evaluate_int_expression(std::unique_ptr<NodeAST>& root) {
    std::string preprocessor_error = "Statement needs to be evaluated to a single int value before compilation.";
    if (auto intNode = dynamic_cast<NodeInt *>(root.get())) {
        return Result<int>(intNode->value);
    } else if (auto unaryExprNode = dynamic_cast<NodeUnaryExpr *>(root.get())) {
        auto operandValueStmt = evaluate_int_expression(unaryExprNode->operand);
        if (operandValueStmt.is_error()) return Result<int>(operandValueStmt.get_error());
        int operandValue = operandValueStmt.unwrap();
        if (unaryExprNode->op.type == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
                                        "Unsupported unary operation. " + preprocessor_error,
                                        root->tok.line, "-", unaryExprNode->op.val, root->tok.file));
    } else if (auto binaryExprNode = dynamic_cast<NodeBinaryExpr *>(root.get())) {
        auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
        if (leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
        int leftValue = leftValueStmt.unwrap();
        auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
        if (rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
        int rightValue = rightValueStmt.unwrap();
        if (binaryExprNode->op == "+") {
            return Result<int>(leftValue + rightValue);
        } else if (binaryExprNode->op == "-") {
            return Result<int>(leftValue - rightValue);
        } else if (binaryExprNode->op == "*") {
            return Result<int>(leftValue * rightValue);
        } else if (binaryExprNode->op == "/") {
            if (rightValue == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero. " + preprocessor_error,
                                                root->tok.line, "", std::to_string(rightValue), root->tok.file));
            }
            return Result<int>(leftValue / rightValue);
        } else if (binaryExprNode->op == "mod") {
            return Result<int>(leftValue % rightValue);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                             root->tok.line, "*, /, -, +, mod", binaryExprNode->op, root->tok.file));
    }
    return Result<int>(CompileError(ErrorType::PreprocessorError,
                                    "Unknown expression statement. No variables allowed. " + preprocessor_error,
                                    root->tok.line, "", "", root->tok.file));
}