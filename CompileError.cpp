//
// Created by Mathias Vatter on 29.08.23.
//

#include "CompileError.h"
#include "Tokenizer/Tokenizer.h"

#include <utility>

CompileError::CompileError(ErrorType type, std::string message, std::string expected, const Token &token)
        : m_type(type), message(std::move(message)), expected(std::move(expected)) {
    line_number = token.line; file_name = token.file; got = token.val; line_position = token.pos;
}

CompileError::CompileError(ErrorType type, std::string message, size_t lineNumber, std::string expected,
                           std::string got, std::string fileName)
        : m_type(type), message(std::move(message)),  expected(std::move(expected)),  got(std::move(got)),
          line_number(lineNumber), file_name(std::move(fileName)) {line_position=0;}

void CompileError::print(ErrorType err) {
    if (got == "\n") {
        got = "linebreak";
    }
    auto error_code = ColorCode::Red;
    if(err == ErrorType::CompileError)
        error_code = ColorCode::Red;
    if(err == ErrorType::CompileWarning)
        error_code = ColorCode::Yellow;
    std::cout << error_code << ColorCode::Bold << error_type_to_string(err) << ColorCode::Reset;
    std::cout << error_code << " [Type: " << ColorCode::Bold << error_type_to_string(m_type) << ColorCode::Reset;
    std::cout << error_code << ", " << "Position: " << file_name;
    if(line_number != -1)
        std::cout << ":" << line_number;
    if(line_position > 0)
        std::cout << ":" << line_position;
    std::cout << "]" << std::endl;
    std::cout << message << " ";
    if(!expected.empty())
        std::cout << "Expected: '" << expected << "', ";
    if(!got.empty())
        std::cout << "got: '" << got << "'";
    std::cout << ColorCode::Reset << std::endl;

    // Zeige die entsprechende Zeile aus der Datei
    if(line_number != -1) {
        std::string in_line = "In line " + std::to_string(line_number) + ": ";
        std::cout << ColorCode::Bold << in_line << ColorCode::Reset;
        std::string line = replace_tabs_with_spaces(get_line_from_file(), 1);
        std::cout << line << std::endl;

        if(line_position > 0) {
            for (int i = 0; i < (in_line.length() + line.length()); i++) {
                if (line_position-1 + in_line.length() == i) {
                    std::cout << "^";
                } else {
                    std::cout << " ";
                }
            }
            std::cout << std::endl;
        }
    }
}

void CompileError::exit(ErrorType err) {
    print(err);
    std::cout << ColorCode::Red << "\nSeems like the compilation exited with a failure." << std::endl;
    std::cout << ColorCode::Reset << "Please report any compiler related bugs by creating an issue providing a minimal viable code snippet at: https://bitbucket.org/MathiasVatter/ksp-compiler/issues" << std::endl;
    ::exit(EXIT_FAILURE);
}

std::string CompileError::get_line_from_file() {
    std::ifstream file(file_name);
    std::string line;
    if (file.is_open()) {
        for (size_t i = 0; i < line_number && std::getline(file, line); ++i) {
            // Nichts tun, nur bis zur gewünschten Zeile lesen
        }
        file.close();
        return line;
    }
    CompileError(ErrorType::FileError, "Unable to open file.", -1, "valid path/valid *.ksp file", file_name, "").exit();
    return "";
}

std::string CompileError::replace_tabs_with_spaces(const std::string &input, int spacesPerTab) {
    std::string output;
    for (char c : input) {
        if (c == '\t') {
            // Fügt spacesPerTab Leerzeichen hinzu, wenn ein Tab gefunden wird
            output += std::string(spacesPerTab, ' ');
        } else {
            // Fügt das aktuelle Zeichen hinzu, wenn es kein Tab ist
            output += c;
        }
    }
    return output;
}
