//
// Created by Mathias Vatter on 29.08.23.
//

#pragma once

#include <string>
#include <iostream>
#include <fstream>

struct ColorCode {
    // Farben
    inline static const std::string Red = "\033[31m";
    inline static const std::string Green = "\033[32m";
    inline static const std::string Yellow = "\033[33m";
    inline static const std::string Blue = "\033[34m";
    inline static const std::string Magenta = "\033[35m";
    inline static const std::string Cyan = "\033[36m";
    inline static const std::string White = "\033[37m";

    // Stile
    inline static const std::string Reset = "\033[0m";
    inline static const std::string Bold = "\033[1m";
    inline static const std::string Italic = "\033[3m";
    inline static const std::string Underline = "\033[4m";

    // Hintergrundfarben
    inline static const std::string BgRed = "\033[41m";
    inline static const std::string BgGreen = "\033[42m";
    inline static const std::string BgYellow = "\033[43m";
    inline static const std::string BgBlue = "\033[44m";
    inline static const std::string BgMagenta = "\033[45m";
    inline static const std::string BgCyan = "\033[46m";
    inline static const std::string BgWhite = "\033[47m";
};

//// ANSI-Escapesequenz für Rot
//const std::string red = "\033[31m";
//
//// ANSI-Escapesequenz zum Zurücksetzen der Farbe
//const std::string reset = "\033[0m";

enum class ErrorType {
    CompileError,
    CompileWarning,
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
    inline CompileError(ErrorType type, std::string message, std::string expected, const struct Token& token);;
    inline CompileError(ErrorType type, std::string message, size_t lineNumber, std::string expected, std::string got,const std::string &fileName)
    : m_type(type), message(std::move(message)),  expected(std::move(expected)),  got(std::move(got)),
	line_number(lineNumber), file_name(fileName) {}

    inline void print(ErrorType err=ErrorType::CompileError) {
		if (got == "\n") {
			got = "linebreak";
		}
        if(err == ErrorType::CompileError)
            std::cout << ColorCode::Red << ColorCode::Bold << error_type_to_string(err) << " " << ColorCode::Reset;
        if(err == ErrorType::CompileWarning)
            std::cout << ColorCode::Yellow << ColorCode::Bold << error_type_to_string(err) << ColorCode::Reset;
        std::cout << ColorCode::Red << "[Type: " << error_type_to_string(m_type);
        std::cout << ", " << "Position: " << file_name;
		if(line_number != -1)
        	std::cout << ":" << line_number;
		std::cout << "]" << std::endl;
        std::cout << message << " ";
		if(!expected.empty())
			std::cout << "Expected: '" << expected << "', ";
		if(!got.empty())
			std::cout << "got: '" << got << "'";
		std::cout << ColorCode::Reset << std::endl;

        // Zeige die entsprechende Zeile aus der Datei
		if(line_number != -1) {
            std::cout << ColorCode::Bold << "In line " << line_number << ": " << ColorCode::Reset;
            std::cout << get_line_from_file() << std::endl;
        }
    }

    inline void exit(ErrorType err=ErrorType::CompileError) {
        print(err);
        std::cout << ColorCode::Red << "\nSeems like the compilation exited with a failure." << std::endl;
        std::cout << ColorCode::Reset << "Please report any compiler related bugs by creating an issue providing a minimal viable code snippet at: https://bitbucket.org/MathiasVatter/ksp-compiler/issues" << std::endl;
        ::exit(EXIT_FAILURE);
    }


private:
    ErrorType m_type;
    std::string message;
	std::string expected;
	std::string got;
	size_t line_number;
    std::string file_name;

    [[nodiscard]] static inline std::string error_type_to_string(ErrorType type) {
        switch(type) {
            case ErrorType::CompileError: return "CompileError";
            case ErrorType::CompileWarning: return "CompileWarning";
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

