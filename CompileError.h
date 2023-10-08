//
// Created by Mathias Vatter on 29.08.23.
//

#pragma once


#include <string>
#include <iostream>
#include <fstream>

// ANSI-Escapesequenz für Rot
const std::string red = "\033[31m";

// ANSI-Escapesequenz zum Zurücksetzen der Farbe
const std::string reset = "\033[0m";

enum class ErrorType {
	SyntaxError,
	TypeError,
	UndefinedVariable,
	TokenError,
    ParseError,
    PreprocessorError
	// TODO weitere Fehlerarten
};

class CompileError {
public:

    inline CompileError(ErrorType type, std::string message, size_t lineNumber, std::string expected, std::string got,const std::string &fileName)
    : type(type), message(std::move(message)),  expected(std::move(expected)),  got(std::move(got)),
	line_number(lineNumber), file_name(fileName) {}

    inline void print() {
		if (got == "\n") {
			got = "linebreak";
		}
        std::cout << red << "CompileError [Type: " << error_type_to_string() << ", File: " << file_name <<
        ", Line: " << line_number << "]: " << std::endl;
        std::cout << message << " Expected: '" << expected << "', got: '"
		<< got << "'" << std::endl;

        // Zeige die entsprechende Zeile aus der Datei
        std::cout << "In line " << line_number << ": " << get_line_from_file() << reset << std::endl;
    }

private:
    ErrorType type;
    std::string message;
	std::string expected;
	std::string got;
	size_t line_number;
    std::string file_name;

    [[nodiscard]] inline std::string error_type_to_string() const {
        switch(type) {
            case ErrorType::SyntaxError: return "SyntaxError";
            case ErrorType::TypeError: return "TypeError";
            case ErrorType::UndefinedVariable: return "UndeclaredVariable";
            case ErrorType::TokenError: return "TokenError";
            case ErrorType::ParseError: return "ParseError";
            case ErrorType::PreprocessorError: return "PreprocessorError";
            // TODO weitere Fehlerarten
        }
    }

    std::string get_line_from_file() {
        std::ifstream file(file_name);
        std::string line;
        if (file.is_open()) {
            for (size_t i = 0; i < line_number && std::getline(file, line); ++i) {
                // Nichts tun, nur bis zur gewünschten Zeile lesen
            }
            file.close();
            return line;
        }
        return "Unable to open file.";
    }
};

