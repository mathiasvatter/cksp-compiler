//
// Created by Mathias Vatter on 25.01.24.
//

#include <iostream>
#include <filesystem>
#include <sstream>

#include "CommandLineOptions.h"
#include "version.h"
#include "Readme.h"

CommandLineOptions::CommandLineOptions(int argc, char **argv) {

    std::string help = R"(
Usage: cksp [options] <input-file>

Options:
 -h, --help                    Display usage information
 -o <file>, --output <file>    Set output file name (default: <input_dir>/out.txt)
 -v, --version                 Display version number

)";
    std::string data(reinterpret_cast<char*>(Readme), Readme_len);
    help += data;
    std::string version = "cksp version "+COMPILER_VERSION+"\n";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            std::cout << help;
            exit(0);
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                m_output_file = argv[++i];
            } else {
                std::cerr << "Error: -o option requires one argument.\n";
                exit(1);
            }
        } else if (arg == "-v" || arg == "--version") {
            std::cout << version;
            exit(0);
        } else {
            m_input_file = arg;
        }
    }

    if (m_input_file.empty()) {
        std::cerr << "Error: No input file provided.\n";
        std::cout << help;
        exit(1);
    }
    if (std::filesystem::path(m_input_file).is_relative()) {
        m_input_file = std::filesystem::path(std::filesystem::current_path() / m_input_file).string();
    }
    m_standard_output_file = std::filesystem::path(std::filesystem::path(m_input_file).parent_path() / "out.txt").string();


}

const std::string &CommandLineOptions::get_input_file() const {
    return m_input_file;
}

const std::string &CommandLineOptions::get_output_file() const {
    return m_output_file;
}

const std::string &CommandLineOptions::get_standard_output_file() const {
    return m_standard_output_file;
}

std::string CommandLineOptions::get_help_option() {
	std::ostringstream ss;
	ss << "Usage: cksp [options] <input-file>" << std::endl;
	ss << std::endl;
	ss << "Options:" << std::endl;
	for(const auto & tuple : m_option_tuples) {
		ss << "-" << std::get<0>(tuple) << ", --" << std::get<1>(tuple) << " " << std::get<2>(tuple) << "\t\t" << std::get<4>(tuple) << std::endl;
	}
	return ss.str();
}
