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
    FileError,
	SyntaxError,
	TypeError,
	Variable,
	TokenError,
    ParseError,
    PreprocessorError,
	MathError
	// TODO weitere Fehlerarten
};

class CompileError {
public:

    inline CompileError(ErrorType type, std::string message, size_t lineNumber, std::string expected, std::string got,const std::string &fileName)
    : type(type), message(std::move(message)),  expected(std::move(expected)),  got(std::move(got)),
	line_number(lineNumber), file_name(fileName) {}

    inline void print() {
//		return;
		if (got == "\n") {
			got = "linebreak";
		}
        std::cout << red << "CompileError [Type: " << error_type_to_string() << ", File: " << file_name;
		if(line_number != -1)
        	std::cout << ", Line: " << line_number;
		std::cout << "]: " << std::endl;
        std::cout << message << " ";
		if(!expected.empty())
			std::cout << "Expected: '" << expected << "', ";
		if(!got.empty())
			std::cout << "got: '" << got << "'";
		std::cout << reset << std::endl;

        // Zeige die entsprechende Zeile aus der Datei
		if(line_number != -1)
        	std::cout << "In line " << line_number << ": " << get_line_from_file() << std::endl;
    }

    inline void exit() {
//		return;
        print();
        ::exit(EXIT_FAILURE);
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
            case ErrorType::Variable: return "Variable";
            case ErrorType::TokenError: return "TokenError";
            case ErrorType::ParseError: return "ParseError";
            case ErrorType::PreprocessorError: return "PreprocessorError";
			case ErrorType::MathError: return "MathError";
            case ErrorType::FileError: return "FileError";
            // TODO weitere Fehlerarten
            default: return "UnknownError";
        }
    };

    inline std::string get_line_from_file() {
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

