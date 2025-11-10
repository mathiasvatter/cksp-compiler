//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once

#include <filesystem>
#include <unordered_map>
#include "CompileError.h"
#include "../utils/StringUtils.h"

enum class FileType {
    ksp,
    nckp,
    cksp,
    txt,
    unknown
};

class FileHandler {
    std::unordered_map<std::string, FileType> m_allowed_filetypes = {
        {".ksp", FileType::ksp},
        {".cksp", FileType::cksp},
        {".nckp", FileType::nckp},
        {".txt", FileType::txt}
    };
    std::string m_input_file;
    std::string m_output;
    FileType m_file_type;

public:
    explicit FileHandler(const std::string &mInputFile);
    static std::string read_file(const std::string& filename);

    [[nodiscard]] const std::string &get_output() const;
    [[nodiscard]] FileType get_file_type() const;

    FileType determine_valid_file_type(const std::string& filepath) {
        const std::string extension = std::filesystem::path(filepath).extension().string();
        const auto it = m_allowed_filetypes.find(extension);
        if(it == m_allowed_filetypes.end()) {
            auto allowed_types = std::string();
            StringUtils::join_apply(
                m_allowed_filetypes,
                [](const auto& f) {
                    return "*"+f.first;
                },
            ",");

            CompileError(ErrorType::FileError, "Unable to open file. Not a valid file type.", -1, "*.ksp or *.nckp",extension, "").exit();
            return FileType::unknown;
        }
        return it->second;
    }
};
