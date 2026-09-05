#pragma once

#include <string>

/*
 * Crash reporting: a report on stderr, and in the file CKSP_CRASH_LOG names.
 *
 * The implementation lives in CrashReporter.cpp so that <windows.h> stays out of every
 * translation unit that only wants to install the handler. It declares the enumerators of
 * TOKEN_INFORMATION_CLASS at global scope - <TokenOrigin> among them - which hides the struct
 * of that name in Token.h, so every unqualified mention of it there names an int instead.
 */
namespace CrashReporter {
	/// Installs the handlers for <mode> ("cli" or "lsp") and records the command line the report
	/// is written with.
	void install(const char* mode, int argc, char* argv[]);

	/// The file the report is also written to, empty unless CKSP_CRASH_LOG names one.
	[[nodiscard]] const std::string& crash_log_path();
}
