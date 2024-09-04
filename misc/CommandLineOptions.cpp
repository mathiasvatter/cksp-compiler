//
// Created by Mathias Vatter on 25.01.24.
//

#include <iostream>
#include <filesystem>
#include <sstream>

#include "CommandLineOptions.h"
#include "version.h"
#include "help.h"

CommandLineOptions::CommandLineOptions(int argc, char **argv) {
	std::string input_file;
	std::string output_file;

    std::string cli_help = R"(
Usage: cksp [options] <input-file>

Options:
 -h, --help                    Display usage information
 -o <file>, --output <file>    Set output file name (default: <input_dir>/out.txt)
 -v, --version                 Display version number

)";
    std::string data(reinterpret_cast<char*>(help), help_len);
	cli_help += data;
    std::string version = "cksp version "+COMPILER_VERSION+"\n";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            std::cout << cli_help;
            exit(0);
        } else if (arg == "-o") {
            if (i + 1 < argc) {
				output_file = argv[++i];
            } else {
                std::cerr << "Error: -o option requires one argument.\n";
                exit(1);
            }
        } else if (arg == "-v" || arg == "--version") {
            std::cout << version;
            exit(0);
        } else {
			input_file = arg;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file provided.\n";
        std::cout << cli_help;
        exit(1);
    }
    if (std::filesystem::path(input_file).is_relative()) {
		input_file = std::filesystem::path(std::filesystem::current_path() / input_file).string();
    }

	m_compiler_config = std::make_unique<CompilerConfig>(
		input_file,
		output_file,
		OptimizationLevel::Standard,
		DebugMode::Basic);

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

CompilerConfig* CommandLineOptions::get_compiler_config() const {
	return m_compiler_config.get();
}


