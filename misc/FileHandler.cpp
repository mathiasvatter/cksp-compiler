//
// Created by Mathias Vatter on 25.01.24.
//

#include "FileHandler.h"
#include <sstream>

FileHandler::FileHandler(const std::string &file) : m_input_file(file) {
    // do not continue if file has no valid filetype
    std::string extension = std::filesystem::path(file).extension().string();
    auto it = m_allowed_filetypes.find(extension);
    if(it == m_allowed_filetypes.end()) {
        CompileError(ErrorType::FileError, "Unable to open file. Not a valid file type.", -1, "*.ksp or *.nckp",extension, "").exit();
    }
    m_file_type = it->second;

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

