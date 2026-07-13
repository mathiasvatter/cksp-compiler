//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once

#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <optional>

#include "../cksp/CompilerConfig.h"

enum class CmdOptions {
    Help,
    Version,
	Lsp,
    Output,
    Compression,
	Optimization,
	CallbackCombining,
	ParameterPassing,
	Obfuscating,
};


class CommandLineOptions {
public:
    CommandLineOptions(int argc, char* argv[]);
    ~CommandLineOptions() = default;

	[[nodiscard]] bool is_lsp_mode() const {
		return m_lsp_mode;
	}

	[[nodiscard]] std::unique_ptr<CompilerConfig> get_compiler_config();

private:
	bool m_lsp_mode = false;
	std::unique_ptr<CompilerConfig> m_compiler_config = nullptr;
	std::vector<std::tuple<std::string, std::string, std::string, CmdOptions, std::string>> m_option_tuples = {
            {"h", "help", "", CmdOptions::Help, "Display usage information"},
			{"", "lsp", "", CmdOptions::Lsp, "Run as a JSON-RPC language server over stdio"},
			{"o", "output", "<file>", CmdOptions::Output, "Write output to <file>. May be specified multiple times. If omitted, default is <input_dir>/out.txt."},
            {"v", "version", "", CmdOptions::Version, "Display version number"},
			{"O", "optimize", "<level>", CmdOptions::Optimization, "Set optimization level: none, simple, standard, aggressive"},
			{"", "-O0", "", CmdOptions::Optimization, "Equivalent to --optimize=none"},
			{"", "-O1", "", CmdOptions::Optimization, "Equivalent to --optimize=simple"},
			{"", "-O2", "", CmdOptions::Optimization, "Equivalent to --optimize=standard"},
			{"", "-O3", "", CmdOptions::Optimization, "Equivalent to --optimize=aggressive"},
			{"c", "combine-callbacks", "", CmdOptions::CallbackCombining, "Combine duplicate callbacks into one (enable)"},
			{"",  "no-combine-callbacks", "", CmdOptions::CallbackCombining, "Disable duplicate callback combining"},
			{"P", "pass-by", "<value|reference>", CmdOptions::ParameterPassing, "Force the parameter passing method for all function parameters. The default is 'value'. To imitate SublimeKSP behavior, use 'reference'."},
			{"s", "max-callback-depth", "<value>", CmdOptions::ParameterPassing, "Set the maximum callback stack depth (default: 1000)"},
			{"", "obfuscate", "", CmdOptions::Obfuscating, "Obfuscate all variable names to a random sequence of characters, making them harder to read. Will also substitute most KSP builtin constants with their internal integer representation."},
    };

	[[nodiscard]] std::string get_help_option() const;
};

