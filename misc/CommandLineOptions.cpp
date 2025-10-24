//
// Created by Mathias Vatter on 25.01.24.
//

#include <iostream>
#include <filesystem>
#include <sstream>

#include "CommandLineOptions.h"
#include "version.h"
#include "help.h"
#include "PathHandler.h"

namespace {
	std::string to_abs_norm(std::string p) {
		std::filesystem::path ph(std::move(p));
		if (ph.is_relative()) ph = std::filesystem::current_path() / ph;
		// weakly_canonical tolerates non-existing paths better than canonical
		return std::filesystem::weakly_canonical(ph).string();
	}
}

CommandLineOptions::CommandLineOptions(int argc, char **argv) {

	m_compiler_config = std::make_unique<CompilerConfig>();
	std::string input_file{};

	static PathHandler handler(Token(), "", "");

    std::string cli_help(reinterpret_cast<char*>(help), help_len);
    std::string version = "cksp version "+COMPILER_VERSION+"\n";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
        	std::cout << get_help_option() << std::endl;
            std::cout << cli_help;
            exit(0);
        } else if (arg == "-o") {
        	// Accept repeated -o occurrences
        	if (i + 1 < argc) {
        		std::string val = argv[++i];
        		if (val.empty()) {
        			std::cerr << "Error: -o requires a file path.\n";
        			std::exit(1);
        		}
        		auto out = to_abs_norm(val);
        		auto result = handler.check_valid_output_file(out);
        		if (result.is_error()) {
					auto error = result.get_error();
					error.m_message = "Invalid output file path provided with -o option. " + error.m_message;
					error.exit();
				}
        		m_compiler_config->outputs.emplace_back(to_abs_norm(val));
        	} else {
        		std::cerr << "Error: -o option requires one argument.\n";
        		std::exit(1);
        	}
        } else if (arg == "-v" || arg == "--version") {
            std::cout << version;
            exit(0);
        } else if ((arg == "-O" || arg == "--optimize") && i + 1 < argc) {
        	const auto it = optimization_level_map.find(argv[++i]);
        	if (it != optimization_level_map.end()) {
        		m_compiler_config->optimization_level = it->second;
        	} else {
		    	std::cerr << "Unknown optimization level: " << argv[++i] << "\n";
	        	exit(1);
        	}
        } else if (arg == "-O0") { m_compiler_config->optimization_level = OptimizationLevel::None;
        } else if (arg == "-O1") { m_compiler_config->optimization_level = OptimizationLevel::Simple;
        } else if (arg == "-O2") { m_compiler_config->optimization_level = OptimizationLevel::Standard;
        } else if (arg == "-O3") { m_compiler_config->optimization_level = OptimizationLevel::Aggressive;
        } else if (arg == "-c" || arg == "--combine-callbacks") {
        	m_compiler_config->combine_callbacks = true;
        } else if (arg == "--no-combine-callbacks") {
        	m_compiler_config->combine_callbacks = false;
        } else if (arg == "-P" || arg == "--pass-by") {
        	// Expect a value next (value|reference)
        	std::string mode;
        	if (arg == "--pass-by" && i + 1 < argc) {
        		mode = argv[++i];
        	} else if (arg == "-P" && i + 1 < argc) {
        		mode = argv[++i];
        	} else {
        		std::cerr << "Error: " << arg << " requires an argument: value|reference\n";
        		std::exit(1);
        	}
        	auto it = parameter_passing_map.find(mode);
        	if (it == parameter_passing_map.end()) {
        		std::cerr << "Error: invalid pass-by mode: " << mode << " (expected: value|reference)\n";
        		std::exit(1);
        	}
        	m_compiler_config->parameter_passing = it->second;
        } else {
        	// First non-option token is the input file
        	if (input_file.empty())
        		input_file = arg;
        	else {
        		// Optional: warn or treat as positional; here we treat it as error to keep CLI strict
        		std::cerr << "Unexpected positional argument: " << arg << "\n";
        		std::exit(1);
        	}
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
	auto error = handler.check_valid_path(input_file);
	if (error.is_error()) {
		auto err = error.get_error();
		err.m_message = "Invalid input file path. " + err.m_message;
		err.exit();
	}


	m_compiler_config->input_filename = input_file;

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

std::unique_ptr<CompilerConfig> CommandLineOptions::get_compiler_config() {
	return std::move(m_compiler_config);
}


