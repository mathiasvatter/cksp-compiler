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

    std::string cli_help(reinterpret_cast<char*>(help), help_len);
    std::string version = "cksp version "+COMPILER_VERSION+"\n";
	auto opt_level = OptimizationLevel::Standard;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
        	std::cout << get_help_option() << std::endl;
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
        } else if ((arg == "-O" || arg == "--optimize") && i + 1 < argc) {
        	const auto it = optimization_level_map.find(argv[++i]);
        	if (it != optimization_level_map.end()) {
        		opt_level = it->second;
        	} else {
		    	std::cerr << "Unknown optimization level: " << argv[++i] << "\n";
	        	exit(1);
        	}
        } else if (arg == "-O0") {
        	opt_level = OptimizationLevel::None;
        } else if (arg == "-O1") {
        	opt_level = OptimizationLevel::Simple;
        } else if (arg == "-O2") {
        	opt_level = OptimizationLevel::Standard;
        } else if (arg == "-O3") {
        	opt_level = OptimizationLevel::Aggressive;
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
		opt_level,
		DebugMode::Basic
	);

}

std::string CommandLineOptions::get_help_option() const {
	std::ostringstream ss;
	ss << "USAGE: cksp [options] <input-file>" << std::endl;
	ss << std::endl;
	ss << "OPTIONS:" << std::endl;
	// for(const auto & tuple : m_option_tuples) {
	// 	ss << "-" << std::get<0>(tuple) << ", --" << std::get<1>(tuple) << " " << std::get<2>(tuple) << "\t\t" << std::get<4>(tuple) << std::endl;
	// }
	for(const auto & tuple : m_option_tuples) {
		ss << "  ";
		if (!std::get<0>(tuple).empty()) ss << "-" << std::get<0>(tuple);
		if (!std::get<1>(tuple).empty()) {
			if (!std::get<0>(tuple).empty()) ss << ", ";
			else ss << "    ";
			ss << std::get<1>(tuple);
		}
		ss << " " << std::get<2>(tuple);
		if (std::get<2>(tuple).empty()) ss << "\t";
		ss << "\t\t" << std::get<4>(tuple) << std::endl;
	}
	return ss.str();
}

CompilerConfig* CommandLineOptions::get_compiler_config() const {
	return m_compiler_config.get();
}


