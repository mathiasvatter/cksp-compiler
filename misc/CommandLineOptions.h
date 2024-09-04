//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once
#include <string>
#include <vector>
#include <filesystem>


enum class CmdOptions {
    Help,
    Version,
    Output,
    Compression
};

// Enum für Optimierungsstufen
enum class OptimizationLevel {
	None,       // Keine Optimierung
	Standard,   // Standard-Optimierung
	Aggressive  // Aggressive Optimierung
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

	CompilerConfig(const std::string& input, const std::string& output, OptimizationLevel optLevel, DebugMode debug)
		: input_filename(input), output_filename(output), optimization_level(optLevel), debug_mode(debug) {
		if(!input_filename.empty())
			standard_output_file = std::filesystem::path(
				std::filesystem::path(input_filename).parent_path() / "out.txt").string();
	}
};


class CommandLineOptions {
public:
    CommandLineOptions(int argc, char* argv[]);
    ~CommandLineOptions() = default;

	CompilerConfig* get_compiler_config() const;

private:
	std::unique_ptr<CompilerConfig> m_compiler_config = nullptr;
	std::vector<std::tuple<std::string, std::string, std::string, CmdOptions, std::string>> m_option_tuples = {
            {"h", "help", "", CmdOptions::Help, "Display usage information"},
            {"o", "output", "<file>", CmdOptions::Output, "Set output file name (default: <input_dir>/out.txt)"},
            {"v", "version", "", CmdOptions::Version, "Display version number"},
    };

	std::string get_help_option();
};


