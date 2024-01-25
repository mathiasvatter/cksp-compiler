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

private:
    std::string m_input_file;
    std::string m_output_file;

    std::vector<std::tuple<std::string, std::string, CmdOptions>> m_option_tuples = {
            {"h", "help", CmdOptions::Help},
            {"o", "output", CmdOptions::Help},
    };
};


