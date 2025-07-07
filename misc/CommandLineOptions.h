//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <unordered_map>


enum class CmdOptions {
    Help,
    Version,
    Output,
    Compression,
	Optimization
};

enum class OptimizationLevel {
	None,       // No optimization
	Simple,     // Simple optimizations only
	Standard,   // Default optimization level
	Aggressive  // Maximal optimization
};
inline const std::unordered_map<std::string, OptimizationLevel> optimization_level_map = {
	{ "none",       OptimizationLevel::None      },
	{ "simple",     OptimizationLevel::Simple    },
	{ "standard",   OptimizationLevel::Standard  },
	{ "aggressive", OptimizationLevel::Aggressive}
};

enum class ParameterPassing {
	ByValue,    // Pass by value
	ByReference // Pass by reference
};
inline const std::unordered_map<std::string, ParameterPassing> parameter_passing_map = {
	{ "value",    ParameterPassing::ByValue     },
	{ "reference", ParameterPassing::ByReference }
};


// Enum für Debugging-Modi
enum class DebugMode {
	Off,    // Debugging ausgeschaltet
	Basic,  // Grundlegendes Debugging
	Full    // Volles Debugging
};

struct CompilerConfig {
	std::string input_filename;
	std::string output_filename;
	std::string standard_output_file;
	OptimizationLevel optimization_level = OptimizationLevel::None;
	DebugMode debug_mode = DebugMode::Off;
	ParameterPassing parameter_passing = ParameterPassing::ByValue;

	CompilerConfig(std::string  input, std::string  output, const OptimizationLevel optLevel, const DebugMode debug)
		: input_filename(std::move(input)), output_filename(std::move(output)), optimization_level(optLevel), debug_mode(debug) {
		if(!input_filename.empty())
			standard_output_file = std::filesystem::path(
				std::filesystem::path(input_filename).parent_path() / "out.txt").string();
	}
};


class CommandLineOptions {
public:
    CommandLineOptions(int argc, char* argv[]);
    ~CommandLineOptions() = default;

	[[nodiscard]] CompilerConfig* get_compiler_config() const;

private:
	std::unique_ptr<CompilerConfig> m_compiler_config = nullptr;
	std::vector<std::tuple<std::string, std::string, std::string, CmdOptions, std::string>> m_option_tuples = {
            {"h", "help", "", CmdOptions::Help, "Display usage information"},
            {"o", "output", "<file>", CmdOptions::Output, "Set output file name (default: <input_dir>/out.txt)"},
            {"v", "version", "", CmdOptions::Version, "Display version number"},
			{"O", "optimize", "<level>", CmdOptions::Optimization, "Set optimization level: none, simple, standard, aggressive"},
			{"", "-O0", "", CmdOptions::Optimization, "Equivalent to --optimize=none"},
			{"", "-O1", "", CmdOptions::Optimization, "Equivalent to --optimize=simple"},
			{"", "-O2", "", CmdOptions::Optimization, "Equivalent to --optimize=standard"},
			{"", "-O3", "", CmdOptions::Optimization, "Equivalent to --optimize=aggressive"},

    };

	[[nodiscard]] std::string get_help_option() const;
};


