//
// Created by Mathias Vatter on 25.01.24.
//

#include "FileHandler.h"
#include <sstream>

FileHandler::FileHandler(const std::string &file) : m_input_file(file) {
    // do not continue if file has no valid filetype
    m_file_type = determine_valid_file_type(file);

    m_output = read_file(file);
}

std::string FileHandler::read_file(const std::string& filename) {
    std::ifstream f(filename);
    std::stringstream buf;
    if (!f.is_open()) {
        CompileError(ErrorType::FileError, "Unable to open file.", -1, "valid path/valid *.ksp file", filename, "").exit();
    } else {
        buf << f.rdbuf();
        f.close();
    }
    return buf.str();
}

const std::string &FileHandler::get_output() const {
    return m_output;
}

FileType FileHandler::get_file_type() const {
    return m_file_type;
}

