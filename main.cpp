#include <cstdlib>

#include "misc/CommandLineOptions.h"
#include "misc/CrashReporter.h"
#include "cksp/Compiler.h"
#include "lsp/LanguageServer.h"

int main(int argc, char* argv[]) {
	std::locale::global(std::locale::classic());
	std::cout.imbue(std::locale::classic());
	std::cin.imbue(std::locale::classic());

	ConsoleDiagnosticSink diagnostics;
	try {
		CommandLineOptions cli_options(argc, argv);
		if (cli_options.is_lsp_mode()) {
			CrashReporter::install("lsp", argc, argv);
			JsonRpcConnection connection(std::cin, std::cout);
			LanguageServer server(connection);
			return server.run();
		}

		CrashReporter::install("cli", argc, argv);
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
