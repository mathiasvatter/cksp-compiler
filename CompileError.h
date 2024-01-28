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
    CompileError(ErrorType type, std::string message, std::string expected, const struct Token& token);
    CompileError(ErrorType type, std::string message, size_t lineNumber, std::string expected, std::string got,std::string fileName);

    void print(ErrorType err=ErrorType::CompileWarning);

    void exit(ErrorType err=ErrorType::CompileError);


private:
    ErrorType m_type;
    std::string message;
	std::string expected;
	std::string got;
	size_t line_number;
    size_t line_position;
    std::string file_name;

    std::string get_line_from_file();

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

    // Funktion, die alle \t in einem String durch Spaces ersetzt
    static std::string replace_tabs_with_spaces(const std::string& input, int spacesPerTab = 4);
};

