//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once
#include <string>
#include <vector>


enum class CmdOptions {
    Help,
    Version,
    Output,
    Compression
};

class CommandLineOptions {
public:
    CommandLineOptions(int argc, char* argv[]);
    ~CommandLineOptions() = default;

    [[nodiscard]] const std::string &get_input_file() const;
    [[nodiscard]] const std::string &get_output_file() const;
    [[nodiscard]] const std::string &get_standard_output_file() const;


private:
    std::string m_input_file;
    std::string m_output_file;
    std::string m_standard_output_file;

    std::vector<std::tuple<std::string, std::string, std::string, CmdOptions, std::string>> m_option_tuples = {
            {"h", "help", "", CmdOptions::Help, "Display usage information"},
            {"o", "output", "<file>", CmdOptions::Output, "Set output file name (default: <input_dir>/out.txt)"},
            {"v", "version", "", CmdOptions::Version, "Display version number"},
    };

	std::string get_help_option();
};


