//
// Created by Mathias Vatter on 29.08.23.
//

#pragma once


#include <string>
#include <iostream>

// ANSI-Escapesequenz für Rot
const std::string red = "\033[31m";

// ANSI-Escapesequenz zum Zurücksetzen der Farbe
const std::string reset = "\033[0m";

class CompileError {
public:
    enum class ErrorType {
        SyntaxError,
        TypeError,
        UndefinedVariable,
        TokenError
        // TODO weitere Fehlerarten
    };

    inline CompileError(ErrorType type, std::string message, int lineNumber, std::string fileName="")
    : type(type), message(std::move(message)), line_number(lineNumber), file_name(std::move(fileName)) {}

    inline void print() const {
        std::cerr << red << "CompileError [Type: " << error_type_to_string() << ", File: " << file_name <<
        ", Line: " << line_number << "]: " << reset << message << std::endl;
    }

private:
    ErrorType type;
    std::string message;
    int line_number;
    std::string file_name;

    inline std::string error_type_to_string() const {
        switch(type) {
            case ErrorType::SyntaxError: return "SyntaxError";
            case ErrorType::TypeError: return "TypeError";
            case ErrorType::UndefinedVariable: return "UndeclaredVariable";
            case ErrorType::TokenError: return "TokenError";
            // TODO weitere Fehlerarten
        }
    }
};

