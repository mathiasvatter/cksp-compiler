//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once

#include <filesystem>
#include <unordered_map>
#include "CompileError.h"

enum class FileType {
    ksp,
    nckp,
    cksp,
    txt,
    unknown
};

class FileHandler {
public:
    static std::string read_file(const std::string& filename);

    explicit FileHandler(const std::string &mInputFile);    std::unordered_map<std::string, FileType> m_allowed_filetypes = {{".ksp", FileType::ksp}, {".cksp", FileType::cksp}, {".nckp", FileType::nckp}};
    [[nodiscard]] const std::string &get_output() const;
    [[nodiscard]] FileType get_file_type() const;
    static FileType determine_file_type(const std::string& filepath) {
        std::filesystem::path path(filepath);
        std::string extension = path.extension().string();
        if (extension == ".ksp") {
            return FileType::ksp;
        }
        if (extension == ".cksp") {
            return FileType::cksp;
        }
        if (extension == ".nckp") {
            return FileType::nckp;
        }
        if (extension == ".txt") {
            return FileType::txt;
        }
        return FileType::unknown;
    }

private:
    std::string m_input_file;
    std::string m_output;
    FileType m_file_type;
};
