//
// Created by Mathias Vatter on 25.01.24.
//

#include <filesystem>
#include <unordered_map>
#include "CompileError.h"

#pragma once

enum class FileType {
    ksp,
    nckp,
    txt
};

class FileHandler {
public:
    static std::string read_file(const std::string& filename);

    explicit FileHandler(const std::string &mInputFile);    std::unordered_map<std::string, FileType> m_allowed_filetypes = {{".ksp", FileType::ksp}, {".nckp", FileType::nckp}};
    [[nodiscard]] const std::string &get_output() const;
    [[nodiscard]] FileType get_file_type() const;

private:
    std::string m_input_file;
    std::string m_output;
    FileType m_file_type;
};
