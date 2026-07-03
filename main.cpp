
#include <cstdlib>

#include "misc/CommandLineOptions.h"
#include "cksp/Compiler.h"




int main(int argc, char* argv[]) {
	std::locale::global(std::locale::classic());
	std::cout.imbue(std::locale::classic());
	std::cin.imbue(std::locale::classic());

	// std::locale::global(std::locale("C"));
	ConsoleDiagnosticSink diagnostics;
	try {
		CommandLineOptions cli_options(argc, argv);
		auto config = cli_options.get_compiler_config();
		Compiler compiler(std::move(config));
		const auto result = compiler.compile(diagnostics);
		return result.success ? EXIT_SUCCESS : EXIT_FAILURE;
	} catch (const CompilationAborted& aborted) {
		// Command-line validation and compiler construction happen before Compiler::compile().
		diagnostics.report(aborted.diagnostic());
		return EXIT_FAILURE;
	}
}
